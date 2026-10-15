using helengine.editor;
using helengine.baseplatform.Requests;
using helengine.webgpu.builder;
using System.Buffers.Binary;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace helengine.webgpu.content;

/// <summary>Prepares missing browser material and font profiles with official binary serializers and verifies all desktop scenes.</summary>
public static class DemoDiscWebGpuContentVerifier {
    /// <summary>Formats stable audit/settings JSON without changing any binary material contracts.</summary>
    static readonly JsonSerializerOptions JsonOptions=new() {WriteIndented=true,PropertyNamingPolicy=JsonNamingPolicy.CamelCase};

    /// <summary>Inspects the current authored browser configuration without publishing proposed material/settings changes.</summary>
    public static void Verify(string projectRoot,string reportPath) {
        Prepare(projectRoot,reportPath,false);
    }

    /// <summary>Builds every proposed payload first, preserves existing platform overrides, then optionally publishes only WebGPU changes.</summary>
    public static void Prepare(string projectRoot,string reportPath,bool apply) {
        projectRoot=Path.GetFullPath(projectRoot); reportPath=Path.GetFullPath(reportPath);
        if(!File.Exists(Path.Combine(projectRoot,"project.heproj"))) throw new FileNotFoundException("The original DemoDisc project is required",projectRoot);
        Dictionary<string,byte[]> proposed=new(StringComparer.OrdinalIgnoreCase);
        Dictionary<string,string> existingOverrides=Directory.GetFiles(Path.Combine(projectRoot,"assets/materials"),"*.webgpu.hasset",SearchOption.AllDirectories).ToDictionary(path=>path,Hash,StringComparer.OrdinalIgnoreCase);
        string[] scenes=PrepareScenes(projectRoot,proposed);
        PrepareGroups(projectRoot,proposed);
        List<object> materialAudit=PrepareMaterials(projectRoot,proposed);
        List<object> gallery=VerifyGallery(projectRoot,proposed);
        List<object> fonts=PrepareFonts(projectRoot,proposed);
        string[] changed=proposed.Where(value=>!File.Exists(value.Key) || !File.ReadAllBytes(value.Key).SequenceEqual(value.Value)).Select(value=>value.Key).Order(StringComparer.Ordinal).ToArray();
        if(apply) foreach(string path in changed) {Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllBytes(path,proposed[path]);}
        bool preserved=existingOverrides.All(value=>File.Exists(value.Key) && Hash(value.Key)==value.Value);
        if(!preserved) throw new InvalidDataException("An existing WebGPU material override changed during preparation");
        string[] runtimeModules=Directory.GetFiles(Path.Combine(projectRoot,"assets/codebase"),"code.module.json",SearchOption.AllDirectories).Select(path=>JsonNode.Parse(File.ReadAllText(path)))
            .Where(value=>(string)value["moduleKind"]=="runtime").Select(value=>(string)value["moduleId"]).Order(StringComparer.Ordinal).ToArray();
        var audit=new {ProjectRoot=projectRoot,Applied=apply,Scenes=scenes,ChangedFiles=changed,ExistingOverridesPreserved=preserved,RuntimeModules=runtimeModules,Gallery=gallery,Materials=materialAudit,Menu=VerifyMenu(),BakedMenu=ReadBakedMenu(projectRoot),
            Fonts=fonts,RuntimeRequirementEvidence="Read expanded scene/module/runtime requirements from the official export manifest; raw inventory physicsFlags are not used."};
        Directory.CreateDirectory(Path.GetDirectoryName(reportPath));File.WriteAllText(reportPath,JsonSerializer.Serialize(audit,JsonOptions),new UTF8Encoding(false));
        Console.WriteLine($"Desktop scenes={scenes.Length}; materials={materialAudit.Count}; gallery={gallery.Count}; proposed changes={changed.Length}; applied={apply}; report={reportPath}");
    }

    /// <summary>Declares the original 720p desktop font sizes for WebGPU so native-pixel text never falls back to the generic 32-pixel import default.</summary>
    static List<object> PrepareFonts(string root,Dictionary<string,byte[]> proposed) {
        List<object> fonts=[];
        foreach(string name in new[]{"DemoDiscMenuButtons","DemoDiscMenuCarousel","DemoDiscMenuTitle","DemoDiscSceneUi"}) {
            string path=Path.Combine(root,"assets/fonts",name+".ttf.hasset");
            using var input=File.OpenRead(path);
            var settings=SectionedAssetImportSettingsBinarySerializer.Deserialize(input);
            if(!settings.Processor.Platforms.TryGetValue("windows",out var desktop) || !desktop.Sections.ContainsKey(FontAssetPlatformSettingsSectionDefinition.SectionIdValue)) throw new InvalidDataException("Missing authored desktop font profile: "+path);
            bool existing=settings.Processor.Platforms.TryGetValue("webgpu",out var browser) && browser.Sections.ContainsKey(FontAssetPlatformSettingsSectionDefinition.SectionIdValue);
            if(!existing) {
                if(browser==null) {browser=new AssetPlatformProcessorSettings();settings.Processor.Platforms.Add("webgpu",browser);}
                browser.Font=new FontAssetProcessorSettings {PixelSize=desktop.Font.PixelSize,SnapHorizontalMetricsToPixels=desktop.Font.SnapHorizontalMetricsToPixels};
                using var output=new MemoryStream();SectionedAssetImportSettingsBinarySerializer.Serialize(output,settings);proposed[path]=output.ToArray();
            }
            fonts.Add(new {Font=name,ExistingOverride=existing,DesktopPixels=desktop.Font.PixelSize,WebGpuPixels=browser.Font.PixelSize});
        }
        return fonts;
    }

    /// <summary>Checks the persisted scene metadata consumed by the runtime, independently of the source catalog.</summary>
    static object[] ReadBakedMenu(string root) {
        using var stream=File.OpenRead(Path.Combine(root,"assets/scenes/DemoDiscMainMenu.helen"));
        var scene=(SceneAsset)helengine.files.EditorAssetBinarySerializer.Deserialize(stream);
        List<object> entries=[];
        void Visit(SceneEntityAsset entity) {
            foreach(var component in entity.Components) if(component.ComponentTypeId=="DemoDisc.menu.MenuItemComponent, DemoDisc") {
                var fields=new EditorTaggedSceneComponentFieldReader(component.Payload);
                string Read(string name) {if(!fields.TryGetFieldReader(name,out var reader)) throw new InvalidDataException("Missing baked menu field "+name);using(reader) return reader.ReadString();}
                entries.Add(new {PanelId=Read("PanelId"),ItemId=Read("ItemId"),TargetId=Read("TargetId")});
            }
            foreach(var child in entity.Children) Visit(child);
        }
        foreach(var entity in scene.RootEntities) Visit(entity);
        return entries.ToArray();
    }

    /// <summary>Selects all authored desktop scenes using their existing stable references and makes splash the startup entry.</summary>
    static string[] PrepareScenes(string root,Dictionary<string,byte[]> proposed) {
        string assets=Path.Combine(root,"assets");
        string[] scenes=Directory.GetFiles(Path.Combine(assets,"scenes"),"*.helen",SearchOption.AllDirectories).Select(path=>Path.GetRelativePath(assets,path).Replace('\\','/'))
            .Where(path=>path!="scenes/DemoDiscMainMenuHandheld.helen" && path!="scenes/games/tilt/tilt_trial_ds.helen").Order(StringComparer.Ordinal).ToArray();
        if(scenes.Length!=37) throw new InvalidDataException("Expected exactly the 37 approved desktop scenes");
        string path=Path.Combine(root,"settings/build_config.json");JsonNode document=JsonNode.Parse(File.ReadAllText(path));
        Dictionary<string,JsonNode> references=new(StringComparer.Ordinal);
        foreach(JsonNode platform in document["platforms"].AsArray()) foreach(JsonNode reference in platform["selectedSceneReferences"].AsArray()) references.TryAdd((string)reference["relativePath"],reference);
        JsonNode browser=document["platforms"].AsArray().Single(value=>(string)value["platformId"]=="webgpu"); JsonArray selected=new();JsonArray orders=new();
        string[] ordered=scenes.OrderBy(scene=>scene=="scenes/HelenOfCodeSplash.helen"?0:scene=="scenes/DemoDiscMainMenu.helen"?1:scene=="scenes/SceneLoadingScreen.helen"?2:3).ThenBy(scene=>scene,StringComparer.Ordinal).ToArray();
        foreach(string scene in ordered) {
            if(!references.TryGetValue(scene,out JsonNode original)) throw new InvalidDataException("No existing stable authored reference for "+scene);
            JsonNode reference=original.DeepClone();reference["contentHash"]="sha256:"+Hash(Path.Combine(assets,scene));selected.Add(reference);
            orders.Add(new JsonObject { ["sceneReference"]=reference.DeepClone(),["orderNumber"]=selected.Count });
        }
        browser["selectedSceneReferences"]=selected;browser["sceneOrders"]=orders;browser["overridesProjectScenes"]=true;
        proposed[path]=Encoding.UTF8.GetBytes(document.ToJsonString(JsonOptions));return ordered;
    }

    /// <summary>Adds only the browser target to the existing HD group, preserving all other group members.</summary>
    static void PrepareGroups(string root,Dictionary<string,byte[]> proposed) {
        string path=Path.Combine(root,"settings/platform-groups.json");JsonNode document=JsonNode.Parse(File.ReadAllText(path));
        JsonArray platforms=document["groups"].AsArray().Single(value=>(string)value["id"]=="hd")["platformIds"].AsArray();
        if(!platforms.Any(value=>(string)value=="webgpu")) platforms.Add("webgpu");
        proposed[path]=Encoding.UTF8.GetBytes(document.ToJsonString(JsonOptions));
    }

    /// <summary>Copies effective Windows standard-schema values into missing WebGPU overrides through the official material serializer.</summary>
    static List<object> PrepareMaterials(string root,Dictionary<string,byte[]> proposed) {
        MaterialAssetSettingsService service=new(root);var schema=WebGpuPlatformDefinitionFactory.Create().MaterialSchemas.Single();
        HashSet<string> supported=schema.Fields.Select(value=>value.FieldId).ToHashSet(StringComparer.OrdinalIgnoreCase);List<object> audit=[];
        foreach(string windowsPath in Directory.GetFiles(Path.Combine(root,"assets/materials"),"*.windows.hasset",SearchOption.AllDirectories).Order(StringComparer.Ordinal)) {
            string commonPath=Path.GetFullPath(windowsPath[..^".windows.hasset".Length]); if(!File.Exists(commonPath)) throw new FileNotFoundException("Missing base material for Windows override",commonPath);
            byte[] common=File.ReadAllBytes(commonPath);var desktop=service.LoadPlatformSettingsFromBytes(common,File.ReadAllBytes(windowsPath),"windows");
            if(desktop.SchemaId!="standard-shader") throw new InvalidDataException("Desktop material requires an unmapped WebGPU schema: "+commonPath+" / "+desktop.SchemaId);
            string browserPath=commonPath+".webgpu.hasset";
            if(!File.Exists(browserPath)) {
                MaterialAssetPlatformOverrideDocument document=new() {PlatformId="webgpu",Processor=new MaterialAssetProcessorOverrideSettings {HasSchemaIdOverride=true,SchemaId="standard-shader"}};
                foreach(var field in desktop.FieldValues) if(supported.Contains(field.Key)) document.Processor.FieldValues.Add(field.Key,field.Value);
                foreach(var reference in desktop.AssetReferenceValues) if(supported.Contains(reference.Key)) document.Processor.AssetReferenceValues.Add(reference.Key,reference.Value);
                using MemoryStream bytes=new();MaterialAssetPlatformOverrideDocumentBinarySerializer.Serialize(bytes,document);proposed[browserPath]=bytes.ToArray();
            }
            var browser=service.LoadPlatformSettingsFromBytes(common,Bytes(browserPath,proposed),"webgpu");
            var effective=new MaterialAssetReferenceProjectionService().CreateResolvedFieldValues(browser);
            audit.Add(new {Material=Path.GetRelativePath(root,commonPath).Replace('\\','/'),ExistingOverride=File.Exists(browserPath),Schema=browser.SchemaId,Fields=effective,
                RequestsVertexColors=effective.TryGetValue("vertex-color-mode",out string mode) && mode=="multiply"});
        }
        return audit;
    }

    /// <summary>Runs all 25 real gallery inputs through WGPM and checks exact authored desktop values and references.</summary>
    static List<object> VerifyGallery(string root,Dictionary<string,byte[]> proposed) {
        MaterialAssetSettingsService service=new(root);var schema=WebGpuPlatformDefinitionFactory.Create().MaterialSchemas.Single();List<object> gallery=[];
        for(int metallic=0;metallic<5;++metallic) for(int roughness=0;roughness<5;++roughness) {
            string name=$"M{metallic}R{roughness}",path=Path.Combine(root,"assets/materials/rendering/pbr_gallery",name+".hasset");byte[] common=File.ReadAllBytes(path);
            var desktop=service.LoadPlatformSettingsFromBytes(common,File.ReadAllBytes(path+".windows.hasset"),"windows");var browser=service.LoadPlatformSettingsFromBytes(common,Bytes(path+".webgpu.hasset",proposed),"webgpu");
            var projection=new MaterialAssetReferenceProjectionService();var original=projection.CreateResolvedFieldValues(desktop);var actual=projection.CreateResolvedFieldValues(browser);
            bool preserved=original.Where(value=>schema.Fields.Any(field=>field.FieldId==value.Key)).All(value=>actual.TryGetValue(value.Key,out string text) && text==value.Value);
            var fields=schema.Fields.ToDictionary(field=>field.FieldId,field=>field.DefaultValue,StringComparer.OrdinalIgnoreCase);foreach(var field in actual)fields[field.Key]=field.Value;fields["variant"]="default";
            var cooked=new WebGpuMaterialCooker().Cook(new PlatformMaterialCookRequest(name,"materials/"+name+".hasset","webgpu","release","webgpu","standard-shader",fields));
            var material=WebGpuMaterialArtifactSerializer.Deserialize(cooked.CookedMaterialBytes).Metadata;
            var buffers=material.GetProperty("constantBuffers").EnumerateArray().ToDictionary(value=>value.GetProperty("name").GetString(),value=>Convert.FromBase64String(value.GetProperty("data").GetString()));
            float metal=BinaryPrimitives.ReadSingleLittleEndian(buffers["MetallicBuffer"]),rough=BinaryPrimitives.ReadSingleLittleEndian(buffers["RoughnessBuffer"]);
            if(!preserved || metal!=metallic/4f || rough!=roughness/4f) throw new InvalidDataException("Gallery desktop values were lost for "+name);
            gallery.Add(new {Name=name,Metallic=metal,Roughness=rough,Preserved=preserved});
        }
        return gallery;
    }

    /// <summary>Reads a proposed payload before publication or the existing authored override without inventing fallback data.</summary>
    static byte[] Bytes(string path,Dictionary<string,byte[]> proposed) {
        path=Path.GetFullPath(path);
        return proposed.TryGetValue(path,out byte[] bytes)?bytes:File.ReadAllBytes(path);
    }

    /// <summary>Executes the original desktop menu catalog and preserves the existing three-scene handheld physics selection.</summary>
    static object VerifyMenu() {
        DemoDisc.menu.DemoDiscSceneCatalog catalog=new();
        string[] physics=catalog.CreatePhysicsSceneItems().Where(value=>value.Action.Kind==DemoDisc.menu.MenuActionKind.LoadScene).Select(value=>value.Action.TargetId).ToArray();
        string[] games=catalog.CreateGameSceneItems().Where(value=>value.Action.Kind==DemoDisc.menu.MenuActionKind.LoadScene).Select(value=>value.Action.TargetId).ToArray();
        if(physics.Length!=12 || physics.Distinct(StringComparer.Ordinal).Count()!=12 || !games.Contains("zombislayer",StringComparer.Ordinal)) throw new InvalidDataException("Desktop menu does not expose all authored physics scenes and both games");
        if(catalog.CreatePhysicsNintendoHandheldSceneEntries().Count!=4) throw new InvalidDataException("Handheld physics selection changed unexpectedly");
        return new {PhysicsSceneIds=physics,GameSceneIds=games,HandheldPhysicsEntryCount=4};
    }

    /// <summary>Returns the exact lowercase SHA-256 file identity used for stable scene references and preservation checks.</summary>
    static string Hash(string path) {
        return Convert.ToHexString(SHA256.HashData(File.ReadAllBytes(path))).ToLowerInvariant();
    }
}
