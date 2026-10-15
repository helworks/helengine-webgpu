#include "web_application.hpp"
#include "web_input_backend.hpp"
#include "web_audio_backend.hpp"
#include "platform/webgpu/webgpu_frame.hpp"
#include "platform/webgpu/webgpu_render_manager_3d.hpp"
#include "platform/webgpu/webgpu_render_manager_2d.hpp"
#include "Core.hpp"
#include "CoreInitializationOptions.hpp"
#include "PlatformInfo.hpp"
#include "HostFileSystemContentStreamSource.hpp"
#include "RuntimeSceneCatalog.hpp"
#include "RuntimeSceneCatalogEntry.hpp"
#include "SceneManager.hpp"
#include "ObjectManager.hpp"
#include "Entity.hpp"
#include "Component.hpp"
#include "TextComponent.hpp"
#include "ICamera.hpp"
#include "SceneLoadMode.hpp"
#include "GeneratedRuntimeModuleRegistration.hpp"
#include "runtime/runtime_scene_catalog_manifest.hpp"
#include "runtime/runtime_startup_manifest.hpp"
#include "runtime/runtime_code_module_manifest.hpp"
#include "IPhysicsRuntime.hpp"
#include "IPhysicsTriggerEventRuntime3D.hpp"
#include "TriggerEvent3D.hpp"
#include "runtime/native_exceptions.hpp"
#include <emscripten.h>
#include <cstring>
#include <stdexcept>
#include <typeinfo>

namespace helengine::webgpu {
void WebApplication::Start() {
    emscripten_set_visibilitychange_callback(this, true, Visibility);
    Device.Initialize("#helengine-canvas", [this] {
        try { Boot(); } catch (const std::exception& error) { Fail(error.what()); }
        catch (Exception* error) { std::string message = error->what(); delete error; Fail(message); }
    }, [this](const std::string& error) { Fail(error); });
}
void WebApplication::Boot() {
    Graphics = new WebGpuFrame(Device); Render2D = new WebGpuRenderManager2D(*Graphics); Render3D = new WebGpuRenderManager3D(*Graphics); Render3D->Ui = Render2D;
    Input = new WebInputBackend();
    int width, height; emscripten_get_canvas_element_size("#helengine-canvas", &width, &height);
    Render3D->AddWindow(0, width, height);
    int dpi = static_cast<int>(96 * emscripten_get_device_pixel_ratio()); Render3D->SetWindowDensityDpi(0, dpi);
    auto* options = new CoreInitializationOptions();
    options->ContentStreamSource = new HostFileSystemContentStreamSource("/content");
    size_t count = 0; auto* source = he_runtime_scene_catalog_entries(&count);
    if (!source || !count) throw std::runtime_error("Runtime scene catalog is empty");
    auto* entries = new Array<RuntimeSceneCatalogEntry*>(count);
    std::string startup;
    for (size_t i = 0; i < count; ++i) {
        (*entries)[i] = new RuntimeSceneCatalogEntry(source[i].SceneId, source[i].CookedRelativePath);
        if (std::strcmp(source[i].CookedRelativePath, he_get_runtime_startup_scene_relative_path()) == 0) startup = source[i].SceneId;
    }
    if (startup.empty()) throw std::runtime_error("Startup scene is missing from the catalog");
    options->SceneCatalog = new RuntimeSceneCatalog(entries);
    options->SceneLoadingBudgetMilliseconds = he_get_runtime_scene_loading_budget_milliseconds();
    options->set_PlatformSettingsConfiguration(he_create_runtime_platform_settings());
    Engine = new Core(options);
    Engine->Initialize(Render3D, Render2D, Input, new PlatformInfo("webgpu", "1.0.0"), options);
    CoreInitialized = true;
    Audio = new WebAudioBackend(); Engine->SetAudioBackend(Audio);
    RegisterGeneratedRuntimeModules(Engine);
    auto* physics=Engine->get_PhysicsRuntime();
    std::string physicsType=physics?typeid(*physics).name():std::string();
    EM_ASM({window['helengineRuntimeConfiguration']=({'sceneCount':$0,'moduleIds':[],'physicsType':UTF8ToString($1),'physicsIdentity':$2});},count,physicsType.c_str(),reinterpret_cast<uintptr_t>(physics));
    size_t moduleCount=0;auto* modules=he_runtime_code_module_entries(&moduleCount);
    for(size_t index=0;index<moduleCount;++index) {EM_ASM({window['helengineRuntimeConfiguration']['moduleIds'].push(UTF8ToString($0));},modules[index].ModuleId);}
    Engine->get_SceneManager()->LoadScene(startup, SceneLoadMode::Single);
    Running = true;
    EM_ASM({ document.getElementById('helengine-status').textContent = 'Running'; document.getElementById('helengine-status').dataset.state = 'running'; });
    emscripten_request_animation_frame_loop(Animate, this);
}
EM_BOOL WebApplication::Animate(double timestamp, void* userdata) {
    auto* self = static_cast<WebApplication*>(userdata); self->Frame(timestamp); return self->Running;
}
EM_BOOL WebApplication::Visibility(int, const EmscriptenVisibilityChangeEvent* event, void* userdata) {
    auto* self = static_cast<WebApplication*>(userdata);
    self->Clock.Reset();
    if (event->hidden && self->Input) self->Input->ReleaseAll();
    return false;
}
void WebApplication::Frame(double timestamp) {
    EM_ASM({ window['helengineHostStage'] = 'animation-frame'; });
    if (!Running) return;
    if (EM_ASM_INT({ return document.hidden; })) { Clock.Reset(); return; }
    try {
        double density = emscripten_get_device_pixel_ratio();
        if (density != Density) {
            double cssWidth, cssHeight;
            if (emscripten_get_element_css_size("#helengine-canvas", &cssWidth, &cssHeight) == EMSCRIPTEN_RESULT_SUCCESS) {
                emscripten_set_canvas_element_size("#helengine-canvas", static_cast<int>(cssWidth * density + 0.5), static_cast<int>(cssHeight * density + 0.5));
                Render3D->SetWindowDensityDpi(0, static_cast<int>(96 * density));
                Density = density;
            }
        }
        int width, height; emscripten_get_canvas_element_size("#helengine-canvas", &width, &height);
        auto size = Render3D->get_MainWindowSize();
        if (size.X != width || size.Y != height) { Render3D->OnWindowResize(0, width, height); }
        EM_ASM({ window['helengineHostStage'] = 'core-update'; });
        Engine->Update(Clock.Advance(timestamp));
        EM_ASM({ window['helengineHostStage'] = 'core-draw'; });
        if (Graphics->Begin(width, height)) { Engine->Draw(); Graphics->End(); }
        Frames++;
        auto* physics=Engine->get_PhysicsRuntime();
        std::string physicsType=physics?typeid(*physics).name():std::string();
        EM_ASM({window['helengineRuntimeConfiguration']['physicsType']=UTF8ToString($0);window['helengineRuntimeConfiguration']['physicsIdentity']=$1;},physicsType.c_str(),reinterpret_cast<uintptr_t>(physics));
        EM_ASM({ window['helengineHostStage'] = 'frame-complete'; });
        EM_ASM({ window['helengineDiagnostics'] = ({'frames': $0, 'sceneIds': [], 'entities': {}, 'components': [], 'texts': [], 'triggerEvents': []}); }, Frames);
        if(auto* triggerRuntime=dynamic_cast<IPhysicsTriggerEventRuntime3D*>(physics)) {
            auto* events=triggerRuntime->get_TriggerEvents();
            for(int32_t index=0;index<events->get_Count();++index) {
                auto* event=events->get_Item(index);
                EM_ASM({window['helengineDiagnostics']['triggerEvents'].push($0);},static_cast<int>(event->get_Kind()));
            }
        }
        auto* scenes = Engine->get_SceneManager()->GetLoadedSceneIds();
        for (int32_t i = 0; i < scenes->Count(); ++i) {
            auto id = (*scenes)[i]; EM_ASM({ window['helengineDiagnostics']['sceneIds'].push(UTF8ToString($0)); }, id.c_str());
        }
        delete scenes;
        auto* entities = Engine->get_ObjectManager()->get_Entities();
        for (int32_t i = 0; i < entities->Count(); ++i) {
            auto* entity = (*entities)[i];
            auto orientation = entity->get_Orientation(); auto position = entity->get_Position();
            EM_ASM({ window['helengineDiagnostics']['entities'][$0] = ({'position': [$1,$2,$3], 'orientation': [$4,$5,$6,$7], 'components': []}); }, entity->get_SceneEntityRuntimeId(), position.X, position.Y, position.Z, orientation.X, orientation.Y, orientation.Z, orientation.W);
            if (!entity->get_IsHierarchyEnabled()) continue;
            auto* components=entity->get_Components();
            for(int32_t index=0;index<components->Count();++index) {
                auto* component=(*components)[index];
                EM_ASM({window['helengineDiagnostics']['components'].push(UTF8ToString($0));window['helengineDiagnostics']['entities'][$1]['components'].push(UTF8ToString($0));},typeid(*component).name(),entity->get_SceneEntityRuntimeId());
                if(auto* text=dynamic_cast<TextComponent*>(component)) {
                    auto value=text->get_Text();
                    EM_ASM({window['helengineDiagnostics']['texts'].push(UTF8ToString($0));},value.c_str());
                }
            }
        }
    } catch (const std::exception& error) { Fail(error.what()); }
    catch (Exception* error) { std::string message = error->what(); delete error; Fail(message); }
}
void WebApplication::Fail(const std::string& message) {
    Running = false;
    EM_ASM({ const error = document.getElementById('helengine-error'); error.textContent = UTF8ToString($0); error.hidden = false; document.getElementById('helengine-status').textContent = 'Stopped'; console.error(error.textContent); }, message.c_str());
}
void WebApplication::Stop() {
    Running = false;
    emscripten_set_visibilitychange_callback(nullptr, true, nullptr);
    if (Engine) { if (CoreInitialized) Engine->Dispose(); delete Engine; Engine = nullptr; CoreInitialized = false; }
    delete Input; Input = nullptr; delete Render3D; Render3D = nullptr; delete Render2D; Render2D = nullptr;
    delete Audio; Audio = nullptr;
    delete Graphics; Graphics = nullptr; Device.Shutdown();
}
}
