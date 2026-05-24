namespace SystemMonitorLogger.Utils;

public static class FileHelper
{
    public static string CreateRunDirectory(string outputDirectory)
    {
        var baseDirectory = Path.GetFullPath(outputDirectory, AppContext.BaseDirectory);
        var safeMachineName = MakeSafeFileName(Environment.MachineName);
        var runName = $"{DateTime.Now:yyyy-MM-dd_HH-mm-ss-fff}_{safeMachineName}_{Environment.ProcessId}";

        for (var i = 0; i < 100; i++)
        {
            var suffix = i == 0 ? string.Empty : $"-{i}";
            var runDirectory = Path.Combine(baseDirectory, runName + suffix);

            try
            {
                Directory.CreateDirectory(runDirectory);
                return runDirectory;
            }
            catch (IOException) when (Directory.Exists(runDirectory))
            {
            }
        }

        throw new IOException("Nao foi possivel criar uma pasta unica para a execucao.");
    }

    private static string MakeSafeFileName(string value)
    {
        var invalidChars = Path.GetInvalidFileNameChars();
        var chars = value.Select(c => invalidChars.Contains(c) ? '-' : c).ToArray();
        return new string(chars);
    }
}
