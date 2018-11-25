using Nuke.Common;
using Nuke.Common.Git;
using Nuke.Common.ProjectModel;
using Nuke.Common.Tools.MSBuild;
using Nuke.Common.Utilities.Collections;
using System;
using System.Collections.Generic;
using System.IO;
using static Nuke.Common.IO.FileSystemTasks;
using static Nuke.Common.IO.PathConstruction;
using static Nuke.Common.Tools.MSBuild.MSBuildTasks;
using RP = Nuke.Common.IO.PathConstruction.RelativePath;

class Build : NukeBuild
{
    public static int Main() => Execute<Build>(x => x.Compile);

    [Parameter("Configuration to build - Default is 'Debug' (local) or 'Release' (server)")]
    readonly string Configuration = IsLocalBuild ? "Debug" : "Release";
    [Solution("HidGuardian.sln")] readonly Solution Solution;
    [GitRepository] readonly GitRepository GitRepository;

    AbsolutePath ArtifactsDirectory => RootDirectory / "artifacts";

    Target Clean => _ => _
        .Executes(() =>
        {
            EnsureCleanDirectory(ArtifactsDirectory);
        });

    Target Restore => _ => _
        .DependsOn(Clean)
        .Executes(() =>
        {
            MSBuild(s => s
                .SetTargetPath(Solution)
                .SetTargets("Restore"));
        });

    Target Compile => _ => _
        .DependsOn(Restore)
        .Executes(() =>
        {
            MSBuild(s => s
                .SetTargetPath(Solution)
                .SetTargets("Rebuild")
                .SetConfiguration(Configuration)
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild)
                .SetTargetPlatform(MSBuildTargetPlatform.x64));

            MSBuild(s => s
                .SetTargetPath(Solution)
                .SetTargets("Rebuild")
                .SetConfiguration(Configuration)
                .SetMaxCpuCount(Environment.ProcessorCount)
                .SetNodeReuse(IsLocalBuild)
                .SetTargetPlatform(MSBuildTargetPlatform.x86));

            #region Transfer all files into common directory for makecab to work

            EnsureExistingDirectory(ArtifactsDirectory / "x64");
            EnsureExistingDirectory(ArtifactsDirectory / "x86");

            new Dictionary<RP, RP>
            {
                { (RP)"driver" / "x64" / "HidGuardian.inf", /* => */ (RP)"HidGuardian.inf" },
                { (RP)"driver" / "x64" / "HidGuardian.pdb", /* => */ (RP)"x64" / "HidGuardian.pdb" },
                { (RP)"driver" / "x64" / "HidGuardian" / "HidGuardian.sys", /* => */ (RP)"x64" / "HidGuardian.sys" },
                { (RP)"driver" / "x64" / "HidGuardian" / "WdfCoinstaller01011.dll", /* => */ (RP)"x64" / "WdfCoinstaller01011.dll" },
                { (RP)"driver" / "x86" / "HidGuardian.pdb", /* => */ (RP)"x86" / "HidGuardian.pdb" },
                { (RP)"driver" / "x86" / "HidGuardian" / "HidGuardian.sys", /* => */ (RP)"x86" / "HidGuardian.sys" },
                { (RP)"driver" / "x86" / "HidGuardian" / "WdfCoinstaller01011.dll", /* => */ (RP)"x86" / "WdfCoinstaller01011.dll" }
            }.ForEach((pair, i) => File.Copy(Solution.Directory / pair.Key, ArtifactsDirectory / pair.Value));

            #endregion
        });
}
