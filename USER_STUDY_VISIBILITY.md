# Independent result-view presentation

ATrajectoryQueryNiagaraActor::SetResultViewVisibility(PathVisible, ObjectVisible)
provides presentation-only gates, defaulting to true/true. The consuming
application owns study configuration, session state and keyboard bindings.

The actor uploads User.PathViewVisible and User.ObjectViewVisible before Niagara
activation. NS_FRNN_Result and NS_FRNN_Result_OnlyQuery bind every renderer's
RendererEnabledBinding to the matching bool, with true defaults. Existing
particle/category/radius/orientation filters are unchanged. The legacy
QueryResultCollisions001 emitter is object view; QueryTrajectory001 in the
query-only system is path view. Disabled LocalQueryDirectionMarker uses object.

RefreshResultViewPresentation applies the states to the Blueprint's
PathVisBox/PathAxes and RelativeVisBox/RelativeAxes. VisibleSampleBoxComponent
retains its bounds and original collision mode while hidden, combines empty and
presentation suppression for axes, and restores collision only when samples
exist. Bounds updates cannot re-enable hidden interaction. Axes widget
preferences continue to use their separate component visibility flag.

RibbonPicking::FParameters.VisibleViews masks samples by view before segment
reduction. A visibility change increments the display revision and selector
session, clears the preview and invalidates pending confirmations. Committed
inspection state is preserved. No query data or preset fields change.

Validation: UE 5.6 Editor and Windows Development game builds pass. D3D12 GPU
picking integration passes, including
live renderer parameter bindings, independent view picking, both hidden,
restoration without simulation reset and stale confirmation cancellation.
The consuming project's UserStudy automation also checks Blueprint axes/grab
collision and empty-view restoration. No headset or real-dataset check performed.
