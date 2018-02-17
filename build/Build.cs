using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Nuke.Common;
using Nuke.Common.Git;
using Nuke.Common.Tools.GitVersion;
using Nuke.Common.Tools.MSBuild;
using Nuke.Core;
using Nuke.Core.Utilities.Collections;
using static Nuke.Common.Tools.MSBuild.MSBuildTasks;
using static Nuke.Core.IO.FileSystemTasks;
using static Nuke.Core.IO.PathConstruction;
using static Nuke.Core.EnvironmentInfo;
using RP = Nuke.Core.IO.PathConstruction.RelativePath;

class Build : NukeBuild
{
    // Console application entry. Also defines the default target.
    public static int Main() => Execute<Build>(x => x.Compile);

    // Auto-injection fields:

    // [GitVersion] readonly GitVersion GitVersion;
    // Semantic versioning. Must have 'GitVersion.CommandLine' referenced.

    // [GitRepository] readonly GitRepository GitRepository;
    // Parses origin, branch name and head from git config.

    // [Parameter] readonly string MyGetApiKey;
    // Returns command-line arguments and environment variables.

    Target Clean => _ => _
            .OnlyWhen(() => false) // Disabled for safety.
            .Executes(() =>
            {
                DeleteDirectories(GlobDirectories(SourceDirectory, "**/bin", "**/obj"));
                EnsureCleanDirectory(OutputDirectory);
            });

    Target Restore => _ => _
            .DependsOn(Clean)
            .Executes(() =>
            {
                MSBuild(s => DefaultMSBuildRestore.SetTargetPlatform(MSBuildTargetPlatform.x64));
                MSBuild(s => DefaultMSBuildRestore.SetTargetPlatform(MSBuildTargetPlatform.x86));
            });

    Target Compile => _ => _
            .DependsOn(Restore)
            .Executes(() =>
            {
                MSBuild(s => DefaultMSBuildCompile.SetTargetPlatform(MSBuildTargetPlatform.x64));
                MSBuild(s => DefaultMSBuildCompile.SetTargetPlatform(MSBuildTargetPlatform.x86));

                #region Ugly hack, fix me!

                EnsureExistingDirectory(ArtifactsDirectory / "x64");
                EnsureExistingDirectory(ArtifactsDirectory / "x86");
                
                new Dictionary<RP, RP>
                {
                    { (RP)"bin" / "x64" / "HidGuardian.inf", /* => */ (RP)"HidGuardian.inf" },
                    { (RP)"bin" / "x64" / "HidGuardian.pdb", /* => */ (RP)"x64" / "HidGuardian.pdb" },
                    { (RP)"bin" / "x64" / "HidGuardian" / "HidGuardian.sys", /* => */ (RP)"x64" / "HidGuardian.sys" },
                    { (RP)"bin" / "x64" / "HidGuardian" / "WdfCoinstaller01009.dll", /* => */ (RP)"x64" / "WdfCoinstaller01009.dll" },
                    { (RP)"bin" / "x86" / "HidGuardian.pdb", /* => */ (RP)"x86" / "HidGuardian.pdb" },
                    { (RP)"bin" / "x86" / "HidGuardian" / "HidGuardian.sys", /* => */ (RP)"x86" / "HidGuardian.sys" },
                    { (RP)"bin" / "x86" / "HidGuardian" / "WdfCoinstaller01009.dll", /* => */ (RP)"x86" / "WdfCoinstaller01009.dll" }
                }.ForEach((pair, i) => File.Copy(SolutionDirectory / pair.Key, ArtifactsDirectory / pair.Value));
                
                #endregion
            });
}
