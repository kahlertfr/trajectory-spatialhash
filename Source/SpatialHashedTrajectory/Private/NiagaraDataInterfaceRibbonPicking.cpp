#include "NiagaraDataInterfaceRibbonPicking.h"
#include "GlobalShader.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraComponent.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraGpuReadbackManager.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraWorldManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "ShaderParameterStruct.h"
#include "TrajectoryQueryNiagaraActor.h"

namespace RibbonPicking
{
static const TCHAR *ShaderSource = TEXT(R"(
RWBuffer<uint> {0}_Buffer;
uint {0}_SampleCount;
uint {0}_Enabled;
void RecordSample_{0}_UEImpureCall(bool Visible,float3 Position,int View,int SampleIndex,int TrajectoryIndex,float Width,out bool Written)
{
 Written=false;
 if ({0}_Enabled==0 || View<0 || View>1 || SampleIndex<0 || SampleIndex>=(int){0}_SampleCount || TrajectoryIndex<0 || !all(isfinite(Position)) || !isfinite(Width)) return;
 uint Base=(View*{0}_SampleCount+SampleIndex)*8;
 {0}_Buffer[Base]=asuint(Position.x); {0}_Buffer[Base+1]=asuint(Position.y); {0}_Buffer[Base+2]=asuint(Position.z);
 {0}_Buffer[Base+3]=Visible ? 1u : 0u;
 {0}_Buffer[Base+4]=TrajectoryIndex+1;
 {0}_Buffer[Base+5]=asuint(abs(Width)*0.5);
 Written=true;
}
)");
BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Buffer)
SHADER_PARAMETER(uint32, SampleCount)
SHADER_PARAMETER(uint32, Enabled)
END_SHADER_PARAMETER_STRUCT()
class FReduceSegmentsCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FReduceSegmentsCS);
	SHADER_USE_PARAMETER_STRUCT(FReduceSegmentsCS, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>, Samples)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Distances)
	SHADER_PARAMETER(uint32, SampleCount)
	SHADER_PARAMETER(uint32, TrajectoryCount)
	SHADER_PARAMETER(FVector3f, ProbePosition)
	SHADER_PARAMETER(float, ProbeRadius)
	SHADER_PARAMETER(FMatrix44f, ComponentLinearTransform)
	END_SHADER_PARAMETER_STRUCT()
};
IMPLEMENT_GLOBAL_SHADER(FReduceSegmentsCS, "/Plugin/SpatialHashedTrajectory/Private/RibbonPicking.usf",
                        "MainCS", SF_Compute);
struct FGameData
{
	FNiagaraParameterDirectBinding<UObject *> Binding;
	FNiagaraParameterDirectBinding<FNiagaraBool> ConfirmationBinding;
};
struct FRenderData
{
	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> Actor;
	uint64 Revision = 0;
	uint64 Session = 0;
	uint64 Frame = 0;
	uint32 SampleCount = 0;
	uint32 TrajectoryCount = 0;
	bool Enabled = false;
	FVector ProbeWorld = FVector::ZeroVector;
	FVector3f ProbeRelative = FVector3f::ZeroVector;
	float Radius = 0;
	FMatrix44f ComponentLinear = FMatrix44f::Identity;
	FNiagaraGpuComputeDispatchInterface *Dispatch = nullptr;
	FDelegateHandle ReadbackHandle;
	FRDGBufferRef Buffer = nullptr;
	FRDGBufferUAVRef UAV = nullptr;
};
struct FProxy : FNiagaraDataInterfaceProxy
{
	TMap<FNiagaraSystemInstanceID, FRenderData> Instances;
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}
	void Readback(FRDGBuilder &GraphBuilder, FNiagaraSystemInstanceID ID,
	              FNiagaraGpuComputeDispatchInterface *Dispatch)
	{
		auto *Data = Instances.Find(ID);
		if (!Data || !Data->Buffer)
			return;

		if (!Data->Enabled || !Data->SampleCount || !Data->TrajectoryCount)
		{
			Data->Buffer = nullptr;
			Data->UAV = nullptr;
			return;
		}
		auto Desc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), Data->TrajectoryCount * 2);
		Desc.Usage |= BUF_SourceCopy;
		auto *Distances = GraphBuilder.CreateBuffer(Desc, TEXT("RibbonPickingDistances"));
		auto *DistanceUAV = GraphBuilder.CreateUAV(Distances, PF_R32_UINT);
		AddClearUAVPass(GraphBuilder, DistanceUAV, 0x7f800000u); // +infinity; nonnegative float bit order
		auto *Parameters = GraphBuilder.AllocParameters<FReduceSegmentsCS::FParameters>();
		Parameters->Samples = GraphBuilder.CreateSRV(Data->Buffer, PF_R32_UINT);
		Parameters->Distances = DistanceUAV;
		Parameters->SampleCount = Data->SampleCount;
		Parameters->TrajectoryCount = Data->TrajectoryCount;
		Parameters->ProbePosition = Data->ProbeRelative;
		Parameters->ProbeRadius = Data->Radius;
		Parameters->ComponentLinearTransform = Data->ComponentLinear;
		TShaderMapRef<FReduceSegmentsCS> Shader(GetGlobalShaderMap(Dispatch->GetFeatureLevel()));
		FComputeShaderUtils::AddPass(GraphBuilder, RDG_EVENT_NAME("TrajectoryRibbonPicking"), Shader,
		                             Parameters,
		                             FComputeShaderUtils::GetGroupCount(Data->SampleCount * 2, 64));
		Dispatch->GetGpuReadbackManager()->EnqueueReadback(
		    GraphBuilder, Distances,
		    [Actor = Data->Actor, Revision = Data->Revision, Session = Data->Session, Frame = Data->Frame,
		     Count = Data->TrajectoryCount,
		     Probe = Data->ProbeWorld](TConstArrayView<TPair<void *, uint32>> Buffers) {
			    if (Buffers.Num() != 1 || Buffers[0].Value < Count * 2 * sizeof(uint32))
				    return;
			    const uint32 *Words = static_cast<const uint32 *>(Buffers[0].Key);
			    uint32 Best = 0x7f800000u;
			    int32 Index = INDEX_NONE;
			    // Stable ties: path, then object; within a view the lowest display index wins.
			    for (uint32 I = 0; I < Count * 2; ++I)
				    if (Words[I] < Best)
				    {
					    Best = Words[I];
					    Index = I % Count;
				    }
			    FNiagaraWorldManager::EnqueueGlobalDeferredCallback(
			        [Actor, Revision, Session, Frame, Index, Probe]() {
				        if (auto *Target = Actor.Get())
					        Target->ReceiveRibbonPick(Revision, Session, Frame, Index, Probe);
			        });
		    });
		Data->Buffer = nullptr;
		Data->UAV = nullptr;
	}
};
} // namespace RibbonPicking

UNiagaraDataInterfaceRibbonPicking::UNiagaraDataInterfaceRibbonPicking(const FObjectInitializer &Initializer)
    : Super(Initializer)
{
	Proxy.Reset(new RibbonPicking::FProxy());
}
void UNiagaraDataInterfaceRibbonPicking::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject))
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()),
		                               ENiagaraTypeRegistryFlags::AllowAnyVariable |
		                                   ENiagaraTypeRegistryFlags::AllowParameter);
}
int32 UNiagaraDataInterfaceRibbonPicking::PerInstanceDataSize() const
{
	return sizeof(RibbonPicking::FGameData);
}
bool UNiagaraDataInterfaceRibbonPicking::InitPerInstanceData(void *Data, FNiagaraSystemInstance *Instance)
{
	new (Data) RibbonPicking::FGameData;
	// A shared DI can be written by several emitters and simulation stages.
	// Read back once after all of them have contributed to this render graph.
	auto *Dispatch = FNiagaraGpuComputeDispatchInterface::Get(Instance->GetWorld());
	ENQUEUE_RENDER_COMMAND(AddRibbonPicking)([P = GetProxyAs<RibbonPicking::FProxy>(), ID = Instance->GetId(),
	                                          Dispatch](FRHICommandListImmediate &) {
		auto &Data = P->Instances.Add(ID);
		Data.Dispatch = Dispatch;
		if (Dispatch)
			Data.ReadbackHandle =
			    Dispatch->GetOnPostRenderEvent().AddRaw(P, &RibbonPicking::FProxy::Readback, ID, Dispatch);
	});
	return true;
}
void UNiagaraDataInterfaceRibbonPicking::DestroyPerInstanceData(void *Data, FNiagaraSystemInstance *Instance)
{
	static_cast<RibbonPicking::FGameData *>(Data)->~FGameData();
	ENQUEUE_RENDER_COMMAND(RemoveRibbonPicking)(
	    [P = GetProxyAs<RibbonPicking::FProxy>(), ID = Instance->GetId()](FRHICommandListImmediate &) {
		    if (auto *Data = P->Instances.Find(ID))
			    if (Data->Dispatch)
				    Data->Dispatch->GetOnPostRenderEvent().Remove(Data->ReadbackHandle);
		    P->Instances.Remove(ID);
	    });
}
bool UNiagaraDataInterfaceRibbonPicking::PerInstanceTick(void *Data, FNiagaraSystemInstance *Instance,
                                                         float Delta)
{
	auto *D = static_cast<RibbonPicking::FGameData *>(Data);
	const FNiagaraVariable Parameter(FNiagaraTypeDefinition(UObject::StaticClass()),
	                                 TEXT("User.RibbonPickingReceiver"));
	auto *Actor =
	    Cast<ATrajectoryQueryNiagaraActor>(D->Binding.Init(Instance->GetInstanceParameters(), Parameter));

	D->ConfirmationBinding.Init(
	    Instance->GetInstanceParameters(),
	    FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("User.ConfirmTrajectorySelection")));
	if (Actor)
		Actor->UpdateRibbonConfirmation(D->ConfirmationBinding.GetValue().GetValue(), GFrameCounter);
	FVector Probe = FVector::ZeroVector;
	float Radius = 0;
	int32 Samples = 0;
	uint64 Session = 0;
	const bool Enabled = Actor && Actor->GetRibbonPickingState(Probe, Radius, Samples, Session);
	const auto *Component = Cast<UNiagaraComponent>(Instance->GetAttachComponent());
	FMatrix44f Linear = FMatrix44f::Identity;
	FVector3f Relative = FVector3f::ZeroVector;
	if (Component)
	{
		FMatrix Matrix = Component->GetComponentTransform().ToMatrixWithScale();
		Matrix.SetOrigin(FVector::ZeroVector);
		Linear = FMatrix44f(Matrix);
		Relative = FVector3f(Probe - Component->GetComponentLocation());
	}
	ENQUEUE_RENDER_COMMAND(UpdateRibbonPicking)(
	    [P = GetProxyAs<RibbonPicking::FProxy>(), ID = Instance->GetId(),
	     Weak = TWeakObjectPtr<ATrajectoryQueryNiagaraActor>(Actor),
	     Revision = Actor ? Actor->GetVisibleBoundsRevision() : 0, Session = Enabled ? Session : 0,
	     Frame = GFrameCounter, Enabled, Samples = Enabled ? Samples : 0,
	     Count = Actor ? Actor->GetInspectableTrajectoryCount() : 0, Probe, Relative,
	     Radius = Enabled ? Radius : 0, Linear](FRHICommandListImmediate &) {
		    if (auto *D = P->Instances.Find(ID))
		    {
			    D->Actor = Weak;
			    D->Revision = Revision;
			    D->Session = Session;
			    D->Frame = Frame;
			    D->Enabled = Enabled;
			    D->SampleCount = Samples;
			    D->TrajectoryCount = Count;
			    D->ProbeWorld = Probe;
			    D->ProbeRelative = Relative;
			    D->Radius = Radius;
			    D->ComponentLinear = Linear;
		    }
	    });
	return false;
}
void UNiagaraDataInterfaceRibbonPicking::BuildShaderParameters(FNiagaraShaderParametersBuilder &Builder) const
{
	Builder.AddNestedStruct<RibbonPicking::FParameters>();
}
void UNiagaraDataInterfaceRibbonPicking::SetShaderParameters(
    const FNiagaraDataInterfaceSetShaderParametersContext &Context) const
{
	auto &D = Context.GetProxy<RibbonPicking::FProxy>().Instances.FindChecked(Context.GetSystemInstanceID());
	auto *Parameters = Context.GetParameterNestedStruct<RibbonPicking::FParameters>();
	if (!Context.IsResourceBound(&Parameters->Buffer))
	{
		Parameters->Buffer = nullptr;
		return;
	}
	Parameters->SampleCount = D.SampleCount;
	Parameters->Enabled = D.Enabled ? 1u : 0u;
	if (!D.Buffer)
	{
		auto Desc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), FMath::Max(1u, D.SampleCount) * 2 * 8);
		Desc.Usage |= BUF_SourceCopy;
		D.Buffer = Context.GetGraphBuilder().CreateBuffer(Desc, TEXT("TrajectoryRibbonPicking"));
		D.UAV = Context.GetGraphBuilder().CreateUAV(D.Buffer, PF_R32_UINT);
		AddClearUAVPass(Context.GetGraphBuilder(), D.UAV, 0);
	}
	Parameters->Buffer = D.UAV;
}
#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceRibbonPicking::GetFunctionsInternal(
    TArray<FNiagaraFunctionSignature> &Functions) const
{
	FNiagaraFunctionSignature S;
	S.Name = TEXT("RecordSample");
	S.bMemberFunction = true;
	S.bRequiresContext = false;
	S.bSupportsCPU = false;
	S.bSupportsGPU = true;
	S.bRequiresExecPin = true;
	S.Inputs = {FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Picker")),
	            FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Visible")),
	            FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")),
	            FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("View")),
	            FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("SampleIndex")),
	            FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("TrajectoryIndex")),
	            FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Width"))};
	S.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Written")));
	Functions.Add(S);
}
void UNiagaraDataInterfaceRibbonPicking::GetParameterDefinitionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo &Info, FString &HLSL)
{
	FString Code = RibbonPicking::ShaderSource;
	Code.ReplaceInline(TEXT("{0}"), *Info.DataInterfaceHLSLSymbol);
	HLSL += Code;
}
bool UNiagaraDataInterfaceRibbonPicking::GetFunctionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo &, const FNiagaraDataInterfaceGeneratedFunction &Function, int,
    FString &)
{
	return Function.DefinitionName == TEXT("RecordSample");
}
bool UNiagaraDataInterfaceRibbonPicking::AppendCompileHash(FNiagaraCompileHashVisitor *Visitor) const
{
	return Super::AppendCompileHash(Visitor) &&
	       Visitor->UpdateString(TEXT("RibbonPickingHLSL"), RibbonPicking::ShaderSource) &&
	       Visitor->UpdateShaderParameters<RibbonPicking::FParameters>();
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRibbonPickingSegmentsTest, "TrajectoryBLab.Query.RibbonPicking.GPUSegments",
                                 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRibbonPickingSegmentsTest::RunTest(const FString &Parameters)
{
	if (GUsingNullRHI)
	{
		AddWarning(TEXT("GPU ribbon picking requires a rendering RHI"));
		return true;
	}
	constexpr uint32 Samples = 12, Trajectories = 6;
	TArray<uint32> Words;
	Words.SetNumZeroed(Samples * 2 * 8);
	const auto Put = [&](int View, int Slot, int Trajectory, FVector3f Position, bool Visible = true,
	                     float HalfWidth = 0.f) {
		uint32 *W = Words.GetData() + (View * Samples + Slot) * 8;
		FMemory::Memcpy(W, &Position, 3 * sizeof(float));
		W[3] = Visible;
		W[4] = Trajectory + 1;
		FMemory::Memcpy(W + 5, &HalfWidth, 4);
	};
	Put(0, 0, 0, {-10, 0, 0});
	Put(0, 1, 0, {10, 0, 0});
	Put(0, 2, 1, {-10, 3, 0});
	Put(0, 3, 1, {10, 3, 0});
	Put(0, 4, 2, {-10, 0, 0}, false);
	Put(0, 5, 2, {10, 0, 0});
	Put(0, 6, 3, {-10, 0, 0});
	Put(0, 7, 4, {10, 0, 0});
	Put(0, 8, 5, {0, 0, 0});
	Put(0, 9, 5, {0, 0, 0});
	Put(1, 0, 1, {-10, 1, 0});
	Put(1, 1, 1, {10, 1, 0});
	Put(1, 2, 2, {-10, 3, 0}, true, 2.5f);
	Put(1, 3, 2, {10, 3, 0}, true, 2.5f);
	const auto Dispatch = [&](FMatrix44f Linear, float Radius) {
		TArray<uint32> Result;
		ENQUEUE_RENDER_COMMAND(
		    TestRibbonSegments)([&Result, Words, Linear, Radius](FRHICommandListImmediate &RHICmdList) {
			FRDGBuilder Graph(RHICmdList);
			auto *Input = Graph.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(4, Words.Num()),
			                                 TEXT("RibbonPickingTestSamples"));
			Graph.QueueBufferUpload(Input, Words.GetData(), Words.Num() * 4, ERDGInitialDataFlags::None);
			auto Desc = FRDGBufferDesc::CreateBufferDesc(4, Trajectories * 2);
			Desc.Usage |= BUF_SourceCopy;
			auto *Output = Graph.CreateBuffer(Desc, TEXT("RibbonPickingTestDistances"));
			auto *UAV = Graph.CreateUAV(Output, PF_R32_UINT);
			AddClearUAVPass(Graph, UAV, 0x7f800000u);
			auto *P = Graph.AllocParameters<RibbonPicking::FReduceSegmentsCS::FParameters>();
			P->Samples = Graph.CreateSRV(Input, PF_R32_UINT);
			P->Distances = UAV;
			P->SampleCount = Samples;
			P->TrajectoryCount = Trajectories;
			P->ProbePosition = FVector3f::ZeroVector;
			P->ProbeRadius = Radius;
			P->ComponentLinearTransform = Linear;
			TShaderMapRef<RibbonPicking::FReduceSegmentsCS> Shader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
			FComputeShaderUtils::AddPass(Graph, RDG_EVENT_NAME("TestRibbonPicking"), Shader, P,
			                             FIntVector(1, 1, 1));
			FRHIGPUBufferReadback Readback(TEXT("RibbonPickingTest"));
			AddEnqueueCopyPass(Graph, &Readback, Output, Trajectories * 2 * 4);
			Graph.Execute();
			RHICmdList.BlockUntilGPUIdle();
			if (Readback.IsReady())
			{
				Result.Append(static_cast<const uint32 *>(Readback.Lock(Trajectories * 2 * 4)),
				              Trajectories * 2);
				Readback.Unlock();
			}
		});
		FlushRenderingCommands();
		return Result;
	};
	const auto Values = Dispatch(FMatrix44f::Identity, 2.f);
	if (!TestEqual(TEXT("GPU distances are read back"), Values.Num(), int32(Trajectories * 2)))
		return false;
	const auto Distance = [&](int View, int Trajectory) {
		float F;
		uint32 W = Values[View * Trajectories + Trajectory];
		FMemory::Memcpy(&F, &W, 4);
		return F;
	};
	TestEqual(TEXT("Mid-segment hit with both samples outside the sphere"), Distance(0, 0), 0.f);
	TestFalse(TEXT("Distant path ribbon misses"), FMath::IsFinite(Distance(0, 1)));
	TestFalse(TEXT("Hidden endpoints do not form selectable segments"), FMath::IsFinite(Distance(0, 2)));
	TestFalse(TEXT("Adjacent trajectories are never joined"),
	          FMath::IsFinite(Distance(0, 3)) || FMath::IsFinite(Distance(0, 4)));
	TestEqual(TEXT("Coincident endpoints are finite"), Distance(0, 5), 0.f);
	TestEqual(TEXT("Object-view segments participate"), Distance(1, 1), 1.f);
	TestEqual(TEXT("Visible ribbon width participates"), Distance(1, 2), 0.25f);
	const FMatrix44f Scale(FScaleMatrix(FVector(2, 3, 4)));
	const auto Scaled = Dispatch(Scale, 2.f);
	TestTrue(TEXT("World radius is unaffected by nonuniform display scale"),
	         Scaled.Num() == 12 && Scaled[7] == 0x7f800000u);
	return true;
}
#endif
