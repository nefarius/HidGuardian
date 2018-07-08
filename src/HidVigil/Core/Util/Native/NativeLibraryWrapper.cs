using System;
using System.IO;
using System.Runtime.InteropServices;
using PInvoke;
using Win32Exception = System.ComponentModel.Win32Exception;

namespace HidVigil.Core.Util.Native
{
    /// <summary>
    ///     Abstract singleton helper class to load native libraries matching the current processor architecture.
    /// </summary>
    /// <typeparam name="T">The type of the derived class.</typeparam>
    public abstract class NativeLibraryWrapper<T> where T : class
    {
        private static readonly Lazy<T> LazyInstance = new Lazy<T>(CreateInstanceOfT);

        /// <summary>
        ///     Object factory.
        /// </summary>
        /// <returns>Returns a new object.</returns>
        private static T CreateInstanceOfT()
        {
            return Activator.CreateInstance(typeof(T), true) as T;
        }

        /// <summary>
        ///     Singleton instance.
        /// </summary>
        public static T Instance => LazyInstance.Value;

        /// <summary>
        ///     Loads a given native library with Windows API function LoadLibrary() considering the process architechture.
        /// </summary>
        /// <param name="name">A short name of the library (appears in logging).</param>
        /// <param name="x86Path">The relative path to the x86-build of the library.</param>
        /// <param name="amd64Path">The relative path to the amd64-build of the library.</param>
        protected void LoadNativeLibrary(string name, string x86Path, string amd64Path)
        {
            // preloading the library matching the current architecture
            if (Environment.Is64BitProcess)
            {
                var lib64 = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, amd64Path);

                if (!Kernel32.LoadLibrary(lib64).IsInvalid)
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
            }
            else
            {
                var lib32 = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, x86Path);

                if (!Kernel32.LoadLibrary(lib32).IsInvalid)
                {
                    throw new Win32Exception(Marshal.GetLastWin32Error());
                }
            }
        }
    }
}
