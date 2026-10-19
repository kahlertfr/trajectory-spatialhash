#include "NiagaraDataInterfaceVisibleBounds.h"
#include "NiagaraCompileHashVisitor.h"
#include "NiagaraGpuComputeDispatchInterface.h"
#include "NiagaraGpuReadbackManager.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraWorldManager.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "TrajectoryQueryNiagaraActor.h"

namespace VisibleBounds
{
static const TCHAR* ShaderSource = TEXT(R"(
uint VBEncode_{0}(float v) { uint b=asuint(v); return (b & 0x80000000u) ? ~b : (b ^ 0x80000000u); }
RWBuffer<uint> {0}_Buffer;
void Accumulate_{0}_UEImpureCall(bool Visible,float3 Position,int View,float Radius,out bool Written)
{
 Written=false;
 if (!Visible || View<0 || View>1 || !all(isfinite(Position)) || !isfinite(Radius)) return;
 uint Base=View*7; float3 Lo=Position-abs(Radius), Hi=Position+abs(Radius);
 if (!all(isfinite(Lo)) || !all(isfinite(Hi))) return;
 [unroll] for (uint Axis=0;Axis<3;++Axis)
 {
  InterlockedMax({0}_Buffer[Base+Axis],~VBEncode_{0}(Lo[Axis]));
  InterlockedMax({0}_Buffer[Base+3+Axis],VBEncode_{0}(Hi[Axis]));
 }
 InterlockedAdd({0}_Buffer[Base+6],1); Written=true;
}
)");
BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, Buffer)
END_SHADER_PARAMETER_STRUCT()
struct FGameData
{
	FNiagaraParameterDirectBinding<UObject*> Binding;
};
struct FRenderData
{
	TWeakObjectPtr<ATrajectoryQueryNiagaraActor> Actor;
	uint64 Revision = 0;
	FNiagaraGpuComputeDispatchInterface* Dispatch = nullptr;
	FDelegateHandle ReadbackHandle;
	FRDGBufferRef Buffer = nullptr;
	FRDGBufferUAVRef UAV = nullptr;
};
static float Decode(uint32 Value)
{
	const uint32 Bits = (Value & 0x80000000u) ? (Value ^ 0x80000000u) : ~Value;
	float Result;
	FMemory::Memcpy(&Result, &Bits, sizeof(Result));
	return Result;
}
struct FProxy : FNiagaraDataInterfaceProxy
{
	TMap<FNiagaraSystemInstanceID, FRenderData> Instances;
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}
	void Readback(FRDGBuilder& GraphBuilder, FNiagaraSystemInstanceID ID, FNiagaraGpuComputeDispatchInterface* Dispatch)
	{
		auto* Data = Instances.Find(ID);
		if (!Data || !Data->Buffer)
			return;
		Dispatch->GetGpuReadbackManager()->EnqueueReadback(
			GraphBuilder, Data->Buffer,
			[Actor = Data->Actor, Revision = Data->Revision](TConstArrayView<TPair<void*, uint32>> Buffers)
			{
				if (Buffers.Num() != 1 || Buffers[0].Value < 14 * sizeof(uint32))
					return;
				const uint32* Words = static_cast<const uint32*>(Buffers[0].Key);
				FBox Boxes[2] = {FBox(ForceInit), FBox(ForceInit)};
				for (int View = 0; View < 2; ++View)
				{
					const uint32* W = Words + View * 7;
					if (W[6])
					{
						FVector Min(Decode(~W[0]), Decode(~W[1]), Decode(~W[2]));
						FVector Max(Decode(W[3]), Decode(W[4]), Decode(W[5]));
						if (!Min.ContainsNaN() && !Max.ContainsNaN())
							Boxes[View] = FBox(Min, Max);
					}
				}
				FNiagaraWorldManager::EnqueueGlobalDeferredCallback(
					[Actor, Revision, Path = Boxes[0], Object = Boxes[1]]()
					{
						if (auto* Target = Actor.Get())
							Target->ReceiveVisibleBounds(Revision, Path, Object);
					});
			});
		Data->Buffer = nullptr;
		Data->UAV = nullptr;
	}
};
} // namespace VisibleBounds

UNiagaraDataInterfaceVisibleBounds::UNiagaraDataInterfaceVisibleBounds(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	Proxy.Reset(new VisibleBounds::FProxy());
}
void UNiagaraDataInterfaceVisibleBounds::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject))
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()),
									   ENiagaraTypeRegistryFlags::AllowAnyVariable |
										   ENiagaraTypeRegistryFlags::AllowParameter);
}
int32 UNiagaraDataInterfaceVisibleBounds::PerInstanceDataSize() const
{
	return sizeof(VisibleBounds::FGameData);
}
bool UNiagaraDataInterfaceVisibleBounds::InitPerInstanceData(void* Data, FNiagaraSystemInstance* Instance)
{
	new (Data) VisibleBounds::FGameData;
	// A shared DI can be written by several emitters and simulation stages.
	// Read back once after all of them have contributed to this render graph.
	auto* Dispatch = FNiagaraGpuComputeDispatchInterface::Get(Instance->GetWorld());
	ENQUEUE_RENDER_COMMAND(AddVisibleBounds)(
		[P = GetProxyAs<VisibleBounds::FProxy>(), ID = Instance->GetId(), Dispatch](FRHICommandListImmediate&)
		{
			auto& Data = P->Instances.Add(ID);
			Data.Dispatch = Dispatch;
			if (Dispatch)
				Data.ReadbackHandle =
					Dispatch->GetOnPostRenderEvent().AddRaw(P, &VisibleBounds::FProxy::Readback, ID, Dispatch);
		});
	return true;
}
void UNiagaraDataInterfaceVisibleBounds::DestroyPerInstanceData(void* Data, FNiagaraSystemInstance* Instance)
{
	static_cast<VisibleBounds::FGameData*>(Data)->~FGameData();
	ENQUEUE_RENDER_COMMAND(RemoveVisibleBounds)(
		[P = GetProxyAs<VisibleBounds::FProxy>(), ID = Instance->GetId()](FRHICommandListImmediate&)
		{
			if (auto* Data = P->Instances.Find(ID))
				if (Data->Dispatch)
					Data->Dispatch->GetOnPostRenderEvent().Remove(Data->ReadbackHandle);
			P->Instances.Remove(ID);
		});
}
bool UNiagaraDataInterfaceVisibleBounds::PerInstanceTick(void* Data, FNiagaraSystemInstance* Instance, float Delta)
{
	auto* D = static_cast<VisibleBounds::FGameData*>(Data);
	const FNiagaraVariable Parameter(FNiagaraTypeDefinition(UObject::StaticClass()),
									 TEXT("User.VisibleBoundsReceiver"));
	auto* Actor = Cast<ATrajectoryQueryNiagaraActor>(D->Binding.Init(Instance->GetInstanceParameters(), Parameter));
	ENQUEUE_RENDER_COMMAND(UpdateVisibleBounds)(
		[P = GetProxyAs<VisibleBounds::FProxy>(), ID = Instance->GetId(),
		 Weak = TWeakObjectPtr<ATrajectoryQueryNiagaraActor>(Actor),
		 Revision = Actor ? Actor->GetVisibleBoundsRevision() : 0](FRHICommandListImmediate&)
		{
			if (auto* D = P->Instances.Find(ID))
			{
				D->Actor = Weak;
				D->Revision = Revision;
			}
		});
	return false;
}
void UNiagaraDataInterfaceVisibleBounds::BuildShaderParameters(FNiagaraShaderParametersBuilder& Builder) const
{
	Builder.AddNestedStruct<VisibleBounds::FParameters>();
}
void UNiagaraDataInterfaceVisibleBounds::SetShaderParameters(
	const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	auto& D = Context.GetProxy<VisibleBounds::FProxy>().Instances.FindChecked(Context.GetSystemInstanceID());
	auto* Parameters = Context.GetParameterNestedStruct<VisibleBounds::FParameters>();
	if (!Context.IsResourceBound(&Parameters->Buffer))
	{
		Parameters->Buffer = nullptr;
		return;
	}
	if (!D.Buffer)
	{
		auto Desc = FRDGBufferDesc::CreateBufferDesc(sizeof(uint32), 14);
		Desc.Usage |= BUF_SourceCopy;
		D.Buffer = Context.GetGraphBuilder().CreateBuffer(Desc, TEXT("TrajectoryVisibleBounds"));
		D.UAV = Context.GetGraphBuilder().CreateUAV(D.Buffer, PF_R32_UINT);
		AddClearUAVPass(Context.GetGraphBuilder(), D.UAV, 0);
	}
	Parameters->Buffer = D.UAV;
}
#if WITH_EDITORONLY_DATA
void UNiagaraDataInterfaceVisibleBounds::GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& Functions) const
{
	FNiagaraFunctionSignature S;
	S.Name = TEXT("Accumulate");
	S.bMemberFunction = true;
	S.bRequiresContext = false;
	S.bSupportsCPU = false;
	S.bSupportsGPU = true;
	S.bRequiresExecPin = true;
	S.Inputs = {FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Bounds")),
				FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Visible")),
				FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")),
				FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("View")),
				FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Radius"))};
	S.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Written")));
	Functions.Add(S);
}
void UNiagaraDataInterfaceVisibleBounds::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& Info,
																	FString& HLSL)
{
	FString Code = VisibleBounds::ShaderSource;
	Code.ReplaceInline(TEXT("{0}"), *Info.DataInterfaceHLSLSymbol);
	HLSL += Code;
}
bool UNiagaraDataInterfaceVisibleBounds::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo&,
														 const FNiagaraDataInterfaceGeneratedFunction& Function, int,
														 FString&)
{
	return Function.DefinitionName == TEXT("Accumulate");
}
bool UNiagaraDataInterfaceVisibleBounds::AppendCompileHash(FNiagaraCompileHashVisitor* Visitor) const
{
	return Super::AppendCompileHash(Visitor) &&
		   Visitor->UpdateString(TEXT("VisibleBoundsHLSL"), VisibleBounds::ShaderSource) &&
		   Visitor->UpdateShaderParameters<VisibleBounds::FParameters>();
}
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVisibleBoundsEncodingTest, "TrajectoryBLab.Query.VisibleBounds.SignedReduction",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FVisibleBoundsEncodingTest::RunTest(const FString& Parameters)
{
	const auto Encode = [](float V)
	{
		uint32 Bits;
		FMemory::Memcpy(&Bits, &V, 4);
		return Bits & 0x80000000u ? ~Bits : Bits ^ 0x80000000u;
	};
	uint32 Min = 0, Max = 0;
	for (float V : {-FLT_MAX, -100.0f, -0.0f, 0.0f, 0.25f, 100.0f, FLT_MAX})
	{
		const uint32 E = Encode(V);
		TestEqual(TEXT("Signed coordinates survive encoding"), VisibleBounds::Decode(E), V);
		Min = FMath::Max(Min, ~E);
		Max = FMath::Max(Max, E);
	}
	TestEqual(TEXT("Zero-initialized minimum handles negative coordinates"), VisibleBounds::Decode(~Min), -FLT_MAX);
	TestEqual(TEXT("Zero-initialized maximum handles positive coordinates"), VisibleBounds::Decode(Max), FLT_MAX);
	return true;
}
#endif
