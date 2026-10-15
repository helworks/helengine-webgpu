using System.Text.Json;

namespace helengine.webgpu.builder.tests;

/// <summary>Checks the actual authored desktop inventory rather than a reduced browser demonstration.</summary>
public sealed class DemoDiscWebGpuInventoryTests {
    /// <summary>Locates the original project whose full desktop content is the browser acceptance target.</summary>
    const string ProjectRoot = "C:/dev/helprojs/demodisc";
    /// <summary>Names the deterministic audit emitted by the isolated editor content utility.</summary>
    const string AuditPath = "C:/dev/helworks/builds/helengine-webgpu/renderer-parity/content-audit.json";

    /// <summary>The selected browser package includes every authored desktop scene and starts at the real splash.</summary>
    [Fact]
    public void Inventory_has_exact_37_desktop_scenes() {
        using JsonDocument settings=JsonDocument.Parse(File.ReadAllText(Path.Combine(ProjectRoot,"settings/build_config.json")));
        var platform=settings.RootElement.GetProperty("platforms").EnumerateArray().Single(value=>value.GetProperty("platformId").GetString()=="webgpu");
        string[] selected=platform.GetProperty("selectedSceneReferences").EnumerateArray().Select(value=>value.GetProperty("relativePath").GetString()).ToArray();
        string root=Path.Combine(ProjectRoot,"assets");
        string[] expected=Directory.GetFiles(Path.Combine(root,"scenes"),"*.helen",SearchOption.AllDirectories)
            .Select(path=>Path.GetRelativePath(root,path).Replace('\\','/')).Where(path=>path!="scenes/DemoDiscMainMenuHandheld.helen" && path!="scenes/games/tilt/tilt_trial_ds.helen").Order(StringComparer.Ordinal).ToArray();
        Assert.Equal(37,expected.Length); Assert.Equal(expected,selected.Order(StringComparer.Ordinal)); Assert.Equal("scenes/HelenOfCodeSplash.helen",selected[0]);
        var orders=platform.GetProperty("sceneOrders");Assert.Equal(37,orders.GetArrayLength());
        Assert.Equal("scenes/HelenOfCodeSplash.helen",orders[0].GetProperty("sceneReference").GetProperty("relativePath").GetString());Assert.Equal(1,orders[0].GetProperty("orderNumber").GetInt32());
        using JsonDocument groups=JsonDocument.Parse(File.ReadAllText(Path.Combine(ProjectRoot,"settings/platform-groups.json")));
        Assert.Contains(groups.RootElement.GetProperty("groups").EnumerateArray().Single(value=>value.GetProperty("id").GetString()=="hd").GetProperty("platformIds").EnumerateArray(),value=>value.GetString()=="webgpu");
    }

    /// <summary>All twenty-five authored metallic/roughness pairs survive the actual WebGPU material cook.</summary>
    [Fact]
    public void Gallery_preserves_25_authored_parameter_sets() {
        Assert.True(File.Exists(AuditPath),"Run the official material content verifier to produce "+AuditPath);
        using JsonDocument audit=JsonDocument.Parse(File.ReadAllText(AuditPath)); var gallery=audit.RootElement.GetProperty("gallery").EnumerateArray().ToArray(); Assert.Equal(25,gallery.Length);
        for(int metallic=0;metallic<5;++metallic) for(int roughness=0;roughness<5;++roughness) {
            var material=gallery.Single(value=>value.GetProperty("name").GetString()==$"M{metallic}R{roughness}");
            Assert.Equal(metallic/4f,material.GetProperty("metallic").GetSingle()); Assert.Equal(roughness/4f,material.GetProperty("roughness").GetSingle()); Assert.True(material.GetProperty("preserved").GetBoolean());
        }
    }

    /// <summary>A second application changes no files and existing browser overrides remain byte-identical.</summary>
    [Fact]
    public void Preparation_is_idempotent_and_preserves_existing_overrides() {
        Assert.True(File.Exists(AuditPath),"Missing actual preparation audit"); using JsonDocument audit=JsonDocument.Parse(File.ReadAllText(AuditPath));
        Assert.True(audit.RootElement.GetProperty("existingOverridesPreserved").GetBoolean()); Assert.Equal(0,audit.RootElement.GetProperty("changedFiles").GetArrayLength());
        Assert.Equal(new[]{"DemoDisc","Raytracing","TiltPlay","Zombislayer"},audit.RootElement.GetProperty("runtimeModules").EnumerateArray().Select(value=>value.GetString()).Order(StringComparer.Ordinal));
        Assert.Equal(12,audit.RootElement.GetProperty("menu").GetProperty("physicsSceneIds").GetArrayLength());
        Assert.Equal(4,audit.RootElement.GetProperty("menu").GetProperty("handheldPhysicsEntryCount").GetInt32());
    }

    /// <summary>Every desktop physics scene and both games have an original engine menu action.</summary>
    [Fact]
    public void All_desktop_physics_scenes_have_menu_actions() {
        string source=File.ReadAllText(Path.Combine(ProjectRoot,"assets/codebase/menu.authoring/DemoDiscSceneCatalog.cs"));
        foreach(string path in Directory.GetFiles(Path.Combine(ProjectRoot,"assets/scenes/physics"),"*.helen")) Assert.Contains('"'+Path.GetFileNameWithoutExtension(path)+'"',source);
        Assert.Contains("CreateDesktopPhysicsSceneEntries",source); Assert.Contains("\"zombislayer\"",source);
        using JsonDocument audit=JsonDocument.Parse(File.ReadAllText(AuditPath));
        var baked=audit.RootElement.GetProperty("bakedMenu").EnumerateArray().ToArray();
        foreach(string path in Directory.GetFiles(Path.Combine(ProjectRoot,"assets/scenes/physics"),"*.helen"))
            Assert.Single(baked.Where(item=>item.GetProperty("targetId").GetString()==Path.GetFileNameWithoutExtension(path)));
        Assert.Single(baked.Where(item=>item.GetProperty("targetId").GetString()=="zombislayer"));
    }
}
