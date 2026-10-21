#pragma once
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRibbonPicking.generated.h"

/** GPU-only picking of segments between already-transformed, visible ribbon samples. */
UCLASS(EditInlineNew, Category = "Trajectory", meta = (DisplayName = "Trajectory ribbon picking"))
class SPATIALHASHEDTRAJECTORY_API UNiagaraDataInterfaceRibbonPicking : public UNiagaraDataInterface
{
	GENERATED_BODY()
  public:
	UNiagaraDataInterfaceRibbonPicking(const FObjectInitializer &Initializer);
	virtual void PostInitProperties() override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override
	{
		return Target == ENiagaraSimTarget::GPUComputeSim;
	}
	virtual int32 PerInstanceDataSize() const override;
	virtual bool InitPerInstanceData(void *Data, FNiagaraSystemInstance *Instance) override;
	virtual void DestroyPerInstanceData(void *Data, FNiagaraSystemInstance *Instance) override;
	virtual bool HasPreSimulateTick() const override
	{
		return true;
	}
	virtual bool PerInstanceTick(void *Data, FNiagaraSystemInstance *Instance, float Delta) override;
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder &Builder) const override;
	virtual void SetShaderParameters(
	    const FNiagaraDataInterfaceSetShaderParametersContext &Context) const override;
#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature> &Functions) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo &Info,
	                                        FString &HLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo &Info,
	                             const FNiagaraDataInterfaceGeneratedFunction &Function, int Index,
	                             FString &HLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor *Visitor) const override;
#endif
};
