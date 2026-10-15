namespace helengine.webgpu.content;

/// <summary>Runs the isolated editor-backed content preparation without loading an editor window.</summary>
public static class Program {
    /// <summary>Requires explicit project/report paths and applies only the requested WebGPU content changes.</summary>
    public static int Main(string[] arguments) {
        if(arguments.Contains("--inspect-tilt-refs")) {
            string root=Value(arguments,"--project-root");
            foreach(string path in Directory.GetFiles(Path.Combine(root,"assets/scenes/games/tilt"),"tilt_trial_level_*.helen")) {
                using var input=new MemoryStream(File.ReadAllBytes(path));var scene=(SceneAsset)helengine.files.EditorAssetBinarySerializer.Deserialize(input);
                List<SceneEntityAsset> entities=[];void Visit(SceneEntityAsset entity){entities.Add(entity);foreach(var child in entity.Children)Visit(child);}foreach(var entity in scene.RootEntities)Visit(entity);
                foreach(var record in entities.SelectMany(entity=>entity.Components).Where(record=>record.ComponentTypeId=="DemoDisc.TiltPlay.TiltTrialSessionComponent, TiltPlay")) {
                    var service=new helengine.editor.ComponentPlatformOverridePayloadService();
                    void Dump(string scope,byte[] payload){var fields=new helengine.editor.EditorTaggedSceneComponentFieldReader(payload);foreach(string name in new[]{"PlayerSphereReference","GoalEntityReference","StageRootReference","TimerTextReference","StartOverlayReference"}) if(fields.TryGetFieldReader(name,out var reader))using(reader){var reference=reader.ReadSceneEntityReference();var target=entities.SingleOrDefault(entity=>entity.Id==reference?.EntityId);Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new {Scene=Path.GetFileName(path),Scope=scope,Field=name,Id=reference?.EntityId,Target=target?.Name,Types=target?.Components.Select(value=>value.ComponentTypeId)}));}}
                    Dump("Common",service.UnwrapBaseRecord(record).Payload);foreach(var state in service.ReadOverrideStates(record))Dump(state.Scope.ToString(),state.Payload);
                }
            }
            return 0;
        }
        if(arguments.Contains("--inspect-light-refs") || arguments.Contains("--repair-light-refs")) {
            string root=Value(arguments,"--project-root");
            foreach(string path in Directory.GetFiles(Path.Combine(root,"assets/scenes/physics"),"*.helen",SearchOption.AllDirectories)) {
                using var stream=new MemoryStream(File.ReadAllBytes(path),false);
                var asset=(helengine.SceneAsset)helengine.files.EditorAssetBinarySerializer.Deserialize(stream);
                List<helengine.SceneEntityAsset> entities=[];
                void Visit(helengine.SceneEntityAsset entity) {entities.Add(entity);foreach(var child in entity.Children) Visit(child);}
                foreach(var entity in asset.RootEntities) Visit(entity);
                bool changed=false;
                foreach(var entity in entities) foreach(var component in entity.Components) if(component.ComponentTypeId=="DemoDisc.rendering.DemoDiscLightToggleComponent, DemoDisc") {
                    var fields=new helengine.editor.EditorTaggedSceneComponentFieldReader(new helengine.editor.ComponentPlatformOverridePayloadService().UnwrapBaseRecord(component).Payload);
                    if(fields.TryGetFieldReader("LightEntityReferences",out var reader)) using(reader) {
                        var references=reader.ReadArray(value=>value.ReadSceneEntityReference());
                        foreach(var reference in references??[]) {
                            var target=entities.SingleOrDefault(value=>value.Id==reference.EntityId);
                            if(target==null || !target.Components.Any(value=>value.ComponentTypeId=="helengine.DirectionalLightComponent")) {
                                if(!arguments.Contains("--repair-light-refs")) throw new InvalidDataException(path+": LightEntityReferences points to "+target?.Name+" instead of a directional light");
                                var keyLight=entities.Single(value=>value.Name=="KeyLight" && value.Components.Any(value=>value.ComponentTypeId=="helengine.DirectionalLightComponent"));
                                var live=new DemoDisc.rendering.DemoDiscLightToggleComponent {LightEntityReferences=[new SceneEntityReference {EntityId=keyLight.Id}]};
                                if(fields.TryGetFieldReader("IndicatorSwatchEntityReference",out var swatch)) using(swatch) live.IndicatorSwatchEntityReference=swatch.ReadSceneEntityReference();
                                if(fields.TryGetFieldReader("UpdateOrder",out var order)) using(order) live.UpdateOrder=order.ReadByte();
                                var descriptor=new helengine.editor.AutomaticScriptComponentPersistenceDescriptor(new helengine.editor.ScriptComponentReflectionSchemaBuilder());
                                var overrides=new helengine.editor.ComponentPlatformOverridePayloadService();
                                var save=new EntityComponentSaveState {ComponentKey=component.ComponentKey};
                                foreach(var state in overrides.ReadOverrideStates(component)) save.SetScopedPlatformOverride(state.Scope,state);
                                var replacement=descriptor.SerializeComponent(live,component.ComponentIndex,save);replacement.ComponentTypeId=component.ComponentTypeId;replacement.ComponentKey=component.ComponentKey;
                                component.Payload=overrides.Wrap(replacement,save).Payload;live.Dispose();changed=true;
                            }
                            Console.WriteLine(System.Text.Json.JsonSerializer.Serialize(new {Scene=Path.GetRelativePath(root,path),Owner=entity.Name,Target=reference.EntityId,Name=target?.Name,Types=target?.Components.Select(value=>value.ComponentTypeId).ToArray()}));
                        }
                    }
                }
                if(changed) {using var output=new MemoryStream();helengine.files.EditorAssetBinarySerializer.Serialize(output,asset);File.WriteAllBytes(path,output.ToArray());Console.WriteLine("Repaired original authored reference: "+path);}
            }
            return 0;
        }
        DemoDiscWebGpuContentVerifier.Prepare(Value(arguments,"--project-root"),Value(arguments,"--report"),arguments.Contains("--apply",StringComparer.Ordinal));
        return 0;
    }

    /// <summary>Reads one required command option rather than guessing a project or output directory.</summary>
    static string Value(string[] arguments,string option) {
        int index=Array.IndexOf(arguments,option);
        if(index<0 || index+1>=arguments.Length || arguments[index+1].StartsWith("--",StringComparison.Ordinal)) throw new ArgumentException("Missing required option "+option);
        return arguments[index+1];
    }
}
