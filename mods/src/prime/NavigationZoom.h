#pragma once

#include <il2cpp/il2cpp_helper.h>

#include "Vector2.h"
#include "Vector3.h"

#include "Camera.h"
#include "NavigationPan.h"

enum class NodeDepth {
  Galaxy       = 1,
  SolarSystem  = 2,
  PlanetSystem = 4,
  Starbase     = 8,
};

struct NavigationZoom {
public:
  void SetViewParameters(float radius, NodeDepth depth)
  {
    static auto SetViewParameters =
        get_class_helper().GetMethod<void(NavigationZoom*, float, NodeDepth)>("SetViewParameters");
    SetViewParameters(this, radius, depth);
  }

  void ZoomCameraAtWorldPoint()
  {
    static auto ZoomCameraAtWorldPoint = get_class_helper().GetMethod<void(NavigationZoom*)>("ZoomCameraAtWorldPoint");
    ZoomCameraAtWorldPoint(this);
  }

  __declspec(property(get = __get_Distance, put = __set_Distance)) float Distance;

  __declspec(property(get = __get__depth, put = __set_depth)) NodeDepth _depth;
  //
  __declspec(property(get = __get__storedZoomDistanceSystem,
                      put = __set__storedZoomDistanceSystem)) float _storedZoomDistanceSystem;
  __declspec(property(get = __get__systemDefaultZoomRatio,
                      put = __set__systemDefaultZoomRatio)) float _systemDefaultZoomRatio;
  __declspec(property(get = __get__actualDistance, put = __set__actualDistance)) float _actualDistance;
  __declspec(property(get = __get__zoomtotal, put = __set__zoomtotal)) float _zoomtotal;
  __declspec(property(get = __get__minimum, put = __set__minimum)) float _minimum;
  __declspec(property(get = __get__maximum, put = __set__maximum)) float _maximum;
  __declspec(property(get = __get__zoomDelta, put = __set__zoomDelta)) float _zoomDelta;
  __declspec(property(get = __get__viewRadius, put = __set__viewRadius)) float _viewRadius;
  __declspec(property(get = __get__lastZoomDelta, put = __set__lastZoomDelta)) float _lastZoomDelta;
  __declspec(property(get = __get__zoomLocation, put = __set__zoomLocation)) vec2 _zoomLocation;
  __declspec(property(get = __get__worldPoint, put = __set__worldPoint)) vec3 _worldPoint;
  __declspec(property(get = __get__farRatioSystemNormal,
                      put = __set__farRatioSystemNormal)) float _farRatioSystemNormal;
  __declspec(property(get = __get__farRatioSystemExtended,
                      put = __set__farRatioSystemExtended)) float _farRatioSystemExtended;

  __declspec(property(get = __get__sceneCamera)) Camera* _sceneCamera;

private:
  static IL2CppClassHelper& get_class_helper()
  {
    static auto class_helper = il2cpp_get_class_helper("Assembly-CSharp", "Digit.Prime.Navigation", "NavigationZoom");
    return class_helper;
  }

public:
  float __get_Distance()
  {
    static auto field = get_class_helper().GetProperty("Distance");
    return *field.Get<float>(this);
  }

  void __set_Distance(float depth)
  {
    static auto field = get_class_helper().GetProperty("Distance");
    field.SetRaw(this, depth);
  }

  NodeDepth __get__depth()
  {
    static auto field = get_class_helper().GetField("_depth");
    return *(NodeDepth*)((ptrdiff_t)this + field.offset());
  }

  void __set_depth(NodeDepth depth)
  {
    static auto field                               = get_class_helper().GetField("_depth");
    *(NodeDepth*)((ptrdiff_t)this + field.offset()) = depth;
  }

  float __get__viewRadius()
  {
    static auto field = get_class_helper().GetField("_viewRadius");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__viewRadius(float delta)
  {
    static auto field                           = get_class_helper().GetField("_viewRadius");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__zoomDelta()
  {
    static auto field = get_class_helper().GetField("_zoomDelta");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__zoomDelta(float delta)
  {
    static auto field                           = get_class_helper().GetField("_zoomDelta");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__minimum()
  {
    static auto field = get_class_helper().GetField("_minimum");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__minimum(float delta)
  {
    static auto field                           = get_class_helper().GetField("_minimum");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__maximum()
  {
    static auto field = get_class_helper().GetField("_maximum");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__maximum(float delta)
  {
    static auto field                           = get_class_helper().GetField("_maximum");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__actualDistance()
  {
    static auto field = get_class_helper().GetField("_actualDistance");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__actualDistance(float delta)
  {
    static auto field                           = get_class_helper().GetField("_actualDistance");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__storedZoomDistanceSystem()
  {
    static auto field = get_class_helper().GetField("_storedZoomDistanceSystem");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__storedZoomDistanceSystem(float delta)
  {
    static auto field                           = get_class_helper().GetField("_storedZoomDistanceSystem");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__systemDefaultZoomRatio()
  {
    static auto field = get_class_helper().GetField("_systemDefaultZoomRatio");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__systemDefaultZoomRatio(float delta)
  {
    static auto field                           = get_class_helper().GetField("_systemDefaultZoomRatio");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__zoomtotal()
  {
    static auto field = get_class_helper().GetField("_zoomtotal");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__zoomtotal(float delta)
  {
    static auto field                           = get_class_helper().GetField("_zoomtotal");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  float __get__lastZoomDelta()
  {
    static auto field = get_class_helper().GetField("_lastZoomDelta");
    return *(float*)((ptrdiff_t)this + field.offset());
  }

  void __set__lastZoomDelta(float delta)
  {
    static auto field                           = get_class_helper().GetField("_lastZoomDelta");
    *(float*)((ptrdiff_t)this + field.offset()) = delta;
  }

  vec2 __get__zoomLocation()
  {
    static auto field = get_class_helper().GetField("_zoomLocation");
    return *(vec2*)((ptrdiff_t)this + field.offset());
  }

  void __set__zoomLocation(vec2 v)
  {
    static auto field                          = get_class_helper().GetField("_zoomLocation");
    *(vec2*)((ptrdiff_t)this + field.offset()) = v;
  }

  vec3 __get__worldPoint()
  {
    static auto field = get_class_helper().GetField("_worldPoint");
    return *(vec3*)((ptrdiff_t)this + field.offset());
  }

  void __set__worldPoint(vec3 v)
  {
    static auto field                          = get_class_helper().GetField("_worldPoint");
    *(vec3*)((ptrdiff_t)this + field.offset()) = v;
  }

  Camera* __get__sceneCamera()
  {
    static auto field = get_class_helper().GetField("_sceneCamera");
    return *(Camera**)((ptrdiff_t)this + field.offset());
  }

  float __get__farRatioSystemNormal()
  {
    static auto field = get_class_helper().GetField("_farRatioSystemNormal").offset();
    return *(float*)((ptrdiff_t)this + field);
  }

  void __set__farRatioSystemNormal(float v)
  {
    static auto field                  = get_class_helper().GetField("_farRatioSystemNormal").offset();
    *(float*)((ptrdiff_t)this + field) = v;
  }

  float __get__farRatioSystemExtended()
  {
    static auto field = get_class_helper().GetField("_farRatioSystemExtended").offset();
    return *(float*)((ptrdiff_t)this + field);
  }

  void __set__farRatioSystemExtended(float v)
  {
    static auto field                  = get_class_helper().GetField("_farRatioSystemExtended").offset();
    *(float*)((ptrdiff_t)this + field) = v;
  }
};
