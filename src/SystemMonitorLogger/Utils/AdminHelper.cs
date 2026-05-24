using System.Runtime.InteropServices;
using System.Security.Principal;

namespace SystemMonitorLogger.Utils;

public static class AdminHelper
{
    public static bool IsElevated()
    {
        if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
        {
            using var identity = WindowsIdentity.GetCurrent();
            var principal = new WindowsPrincipal(identity);
            return principal.IsInRole(WindowsBuiltInRole.Administrator);
        }

        if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
        {
            return Environment.UserName == "root";
        }

        return false;
    }
}
