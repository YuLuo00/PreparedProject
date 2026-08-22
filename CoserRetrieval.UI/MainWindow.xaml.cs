using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace CoserRetrieval.UI;

public partial class MainWindow : Window
{
    public ObservableCollection<TaskRow> Tasks { get; } = new();
    public string NativeStatus { get; private set; }

    public MainWindow()
    {
        InitializeComponent();
        NativeStatus = ReadNativeStatus();
        CreateTestTasks();
        DataContext = this;
        Loaded += (_, _) => NativeStatus = ReadNativeStatus(Path.Combine(FindRegressionDirectory(), "rioko_ref.jpg"));
    }

    private static string ReadNativeStatus(string? samplePath = null)
    {
        var buffer = new System.Text.StringBuilder(256);
        if (NativeMethods.GetBuildInfo(buffer, buffer.Capacity) != 0)
            return "Native bridge unavailable";
        if (samplePath is null || !File.Exists(samplePath)) return buffer.ToString();
        var md5 = new System.Text.StringBuilder(64);
        return NativeMethods.ComputeMd5(samplePath, md5, md5.Capacity) == 0
            ? $"{buffer} | MD5 {md5.ToString()[..8]}..."
            : "Native bridge loaded, MD5 test failed";
    }

    private void CreateTestTasks()
    {
        string images = FindRegressionDirectory();
        NativeStatus = ReadNativeStatus(Path.Combine(images, "rioko_ref.jpg"));
        foreach (string path in Directory.EnumerateFiles(images, "*.*"))
            Tasks.Add(new TaskRow(path, DemoOutcome.Success));
    }

    private async void RunRegression_Click(object sender, RoutedEventArgs e)
    {
        string root = FindProjectRoot();
        string data = Path.Combine(root, "CoserRetrieval", "data");
        Directory.CreateDirectory(data);
        foreach (string suffix in new[] { ".db", "_face.index", "_clothing.index" })
        {
            string file = Path.Combine(data, "ui_regression" + suffix);
            if (File.Exists(file)) File.Delete(file);
        }

        CommandBox.SelectedIndex = 0;
        InputPathBox.Text = FindRegressionDirectory();
        DbPathBox.Text = Path.Combine(data, "ui_regression.db");
        FaceIndexBox.Text = Path.Combine(data, "ui_regression_face.index");
        ClothingIndexBox.Text = Path.Combine(data, "ui_regression_clothing.index");
        PersonBox.Text = "ui_regression";
        RoleBox.Text = "";
        await StartConfiguredTaskAsync();
    }

    private async void StartTask_Click(object sender, RoutedEventArgs e) => await StartConfiguredTaskAsync();

    private void BrowseInput_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new Microsoft.Win32.OpenFolderDialog { Title = "Select image directory" };
        if (dialog.ShowDialog() == true) InputPathBox.Text = dialog.FolderName;
    }

    private async System.Threading.Tasks.Task StartConfiguredTaskAsync()
    {
        string root = FindProjectRoot();
        string input = ResolvePath(root, InputPathBox.Text);
        if (!Directory.Exists(input) && !File.Exists(input))
        {
            MessageBox.Show("Input path does not exist.", "Coser Retrieval", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        string command = ((ComboBoxItem)CommandBox.SelectedItem).Content.ToString()!;
        PopulateTasks(input);
        var args = new System.Collections.Generic.List<string> {
            command, "--db", ResolvePath(root, DbPathBox.Text),
            "--index", ResolvePath(root, FaceIndexBox.Text),
            "--clothing-index", ResolvePath(root, ClothingIndexBox.Text),
            "--models-dir", ResolvePath(root, ModelsPathBox.Text),
            "--task-id", "wpf_task"
        };
        if (Directory.Exists(input)) { args.Add("--dir"); args.Add(input); }
        else { args.Add("--image"); args.Add(input); }
        if (command == "ingest")
        {
            if (!string.IsNullOrWhiteSpace(PersonBox.Text)) { args.Add("--person"); args.Add(PersonBox.Text); }
            if (!string.IsNullOrWhiteSpace(RoleBox.Text)) { args.Add("--role"); args.Add(RoleBox.Text); }
        }
        else
        {
            args.Add("--mode"); args.Add(((ComboBoxItem)QueryModeBox.SelectedItem).Content.ToString()!);
            args.Add("--topk"); args.Add(TopKBox.Text);
        }
        args.AddRange(ParseAdditionalArguments(AdvancedArgsBox.Text));
        StartButton.IsEnabled = false;
        try
        {
            int exitCode = await RunCliAsync(root, args.ToArray());
            foreach (TaskRow task in Tasks) task.FinishIfPending(exitCode == 0);
        }
        finally { StartButton.IsEnabled = true; }
    }

    private void PopulateTasks(string input)
    {
        Tasks.Clear();
        if (Directory.Exists(input))
        {
            foreach (string path in Directory.EnumerateFiles(input, "*.*", SearchOption.AllDirectories))
            {
                string extension = Path.GetExtension(path).ToLowerInvariant();
                if (extension is ".jpg" or ".jpeg" or ".png" or ".bmp" or ".webp") Tasks.Add(new TaskRow(path, DemoOutcome.Success));
            }
        }
        else Tasks.Add(new TaskRow(input, DemoOutcome.Success));
    }

    private async System.Threading.Tasks.Task<int> RunCliAsync(string root, string[] arguments)
    {
        var start = new ProcessStartInfo(Path.Combine(root, "bin", "coser_cli.exe")) {
            WorkingDirectory = root,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true,
        };
        foreach (string argument in arguments) start.ArgumentList.Add(argument);
        using Process process = Process.Start(start) ?? throw new InvalidOperationException("Failed to start coser_cli");
        process.OutputDataReceived += (_, e) => { if (e.Data is not null) Dispatcher.Invoke(() => HandleCliLine(e.Data)); };
        process.ErrorDataReceived += (_, e) => { if (e.Data is not null) Dispatcher.Invoke(() => HandleCliLine(e.Data)); };
        process.BeginOutputReadLine();
        process.BeginErrorReadLine();
        await process.WaitForExitAsync();
        return process.ExitCode;
    }

    private void HandleCliLine(string line)
    {
        int pathIndex = line.LastIndexOf("path=", StringComparison.Ordinal);
        string? path = pathIndex >= 0 ? line[(pathIndex + 5)..].Trim() : null;
        if (path is null)
        {
            int inIndex = line.LastIndexOf(" in ", StringComparison.Ordinal);
            if (inIndex >= 0) path = line[(inIndex + 4)..].Trim();
        }
        if (path is null) return;
        TaskRow? task = Tasks.FirstOrDefault(item => string.Equals(item.FileName, Path.GetFileName(path), StringComparison.OrdinalIgnoreCase));
        if (task is null) return;
        if (line.StartsWith("Ingested", StringComparison.Ordinal)) task.CompleteAll();
        else if (line.Contains("partial route failures", StringComparison.OrdinalIgnoreCase)) task.MarkPartial(line);
        else if (line.Contains("failed", StringComparison.OrdinalIgnoreCase)) task.FailAll();
    }

    private static string FindRegressionDirectory()
    {
        return Path.Combine(FindProjectRoot(), "CoserRetrieval", "testdata", "regression");
    }

    private static string ResolvePath(string root, string value) =>
        Path.IsPathRooted(value) ? value : Path.GetFullPath(Path.Combine(root, value));

    private static System.Collections.Generic.IEnumerable<string> ParseAdditionalArguments(string value)
    {
        foreach (Match match in Regex.Matches(value, "\\\"([^\\\"]*)\\\"|(\\S+)"))
            yield return match.Groups[1].Success ? match.Groups[1].Value : match.Groups[2].Value;
    }

    private static string FindProjectRoot()
    {
        DirectoryInfo? directory = new(AppContext.BaseDirectory);
        while (directory is not null)
        {
            if (Directory.Exists(Path.Combine(directory.FullName, "CoserRetrieval"))) return directory.FullName;
            directory = directory.Parent;
        }
        return AppContext.BaseDirectory;
    }
}

internal static class NativeMethods
{
    [DllImport("coser_bridge.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CoserBridge_GetBuildInfo(System.Text.StringBuilder output, int capacity);

    [DllImport("coser_bridge.dll", CharSet = CharSet.Unicode, CallingConvention = CallingConvention.Cdecl)]
    private static extern int CoserBridge_ComputeMd5(string filePath, System.Text.StringBuilder output, int capacity);

    internal static int GetBuildInfo(System.Text.StringBuilder output, int capacity) =>
        CoserBridge_GetBuildInfo(output, capacity);
    internal static int ComputeMd5(string filePath, System.Text.StringBuilder output, int capacity) =>
        CoserBridge_ComputeMd5(filePath, output, capacity);
}

public enum DemoOutcome { Success, MiddleFailure, TotalFailure }

public sealed class TaskRow : Bindable
{
    private readonly DemoOutcome _outcome;
    private readonly int _failureStage;
    private string _summary = "QUEUED";
    private Brush _summaryBrush = Brushes.Gray;

    public TaskRow(string fullPath, DemoOutcome outcome)
    {
        FullPath = fullPath;
        FileName = Path.GetFileName(fullPath);
        _outcome = outcome;
        _failureStage = outcome == DemoOutcome.MiddleFailure ? 3 : outcome == DemoOutcome.TotalFailure ? 0 : -1;
        Thumbnail = File.Exists(fullPath) ? new BitmapImage(new Uri(fullPath)) : null;
        foreach (string name in new[] { "Hash", "Decode", "Face", "Segment", "Embedding", "Index" })
            Stages.Add(new StageRow(name));
    }

    public string FullPath { get; }
    public string FileName { get; }
    public BitmapImage? Thumbnail { get; }
    public ObservableCollection<StageRow> Stages { get; } = new();
    public string Summary { get => _summary; private set => Set(ref _summary, value); }
    public Brush SummaryBrush { get => _summaryBrush; private set => Set(ref _summaryBrush, value); }

    public void Advance(int tick)
    {
        if (tick == 1) { Start(0); return; }
        int previous = tick - 2;
        if (previous >= 0 && previous < Stages.Count) Stages[previous].Complete();

        int current = tick - 1;
        if (_failureStage == current)
        {
            Stages[current].Fail();
            for (int i = current + 1; i < Stages.Count; ++i) Stages[i].Cancel();
            Summary = "FAILED";
            SummaryBrush = new SolidColorBrush(Color.FromRgb(239, 104, 104));
            return;
        }
        if (current < Stages.Count) { Start(current); return; }
        if (_failureStage < 0)
        {
            Summary = "COMPLETED";
            SummaryBrush = new SolidColorBrush(Color.FromRgb(91, 207, 135));
        }
    }

    public void CompleteAll()
    {
        foreach (StageRow stage in Stages) stage.Complete();
        Summary = "COMPLETED";
        SummaryBrush = new SolidColorBrush(Color.FromRgb(91, 207, 135));
    }

    public void MarkPartial(string detail)
    {
        if (detail.Contains("face:", StringComparison.OrdinalIgnoreCase)) Stages[2].Fail();
        if (detail.Contains("clothing:", StringComparison.OrdinalIgnoreCase)) Stages[3].Fail();
        Summary = "COMPLETED WITH WARNINGS";
        SummaryBrush = new SolidColorBrush(Color.FromRgb(238, 184, 80));
    }

    public void FailAll()
    {
        foreach (StageRow stage in Stages) stage.Fail();
        Summary = "FAILED";
        SummaryBrush = new SolidColorBrush(Color.FromRgb(239, 104, 104));
    }

    public void FinishIfPending(bool succeeded)
    {
        if (Summary == "QUEUED" || Summary == "RUNNING")
        {
            if (succeeded) CompleteAll(); else FailAll();
        }
    }

    private void Start(int index)
    {
        if (_outcome == DemoOutcome.TotalFailure && index > 0) return;
        Stages[index].Start();
        Summary = "RUNNING";
        SummaryBrush = new SolidColorBrush(Color.FromRgb(85, 199, 247));
    }
}

public sealed class StageRow : Bindable
{
    private string _stateText = "WAIT";
    private Brush _background = new SolidColorBrush(Color.FromRgb(39, 45, 52));
    private Brush _borderBrush = new SolidColorBrush(Color.FromRgb(58, 66, 76));
    private Brush _foreground = new SolidColorBrush(Color.FromRgb(126, 136, 146));
    private bool _isRunning;

    public StageRow(string name) => Name = name;
    public string Name { get; }
    public string StateText { get => _stateText; private set => Set(ref _stateText, value); }
    public Brush Background { get => _background; private set => Set(ref _background, value); }
    public Brush BorderBrush { get => _borderBrush; private set => Set(ref _borderBrush, value); }
    public Brush Foreground { get => _foreground; private set => Set(ref _foreground, value); }
    public bool IsRunning { get => _isRunning; private set => Set(ref _isRunning, value); }

    public void Start() { StateText = "RUN"; IsRunning = true; Foreground = new SolidColorBrush(Color.FromRgb(85, 199, 247)); }
    public void Complete() { if (StateText == "FAIL") return; StateText = "DONE"; IsRunning = false; Background = new SolidColorBrush(Color.FromRgb(30, 67, 51)); BorderBrush = new SolidColorBrush(Color.FromRgb(69, 143, 98)); Foreground = new SolidColorBrush(Color.FromRgb(105, 223, 145)); }
    public void Fail() { StateText = "FAIL"; IsRunning = false; Background = new SolidColorBrush(Color.FromRgb(74, 39, 43)); BorderBrush = new SolidColorBrush(Color.FromRgb(164, 72, 79)); Foreground = new SolidColorBrush(Color.FromRgb(245, 114, 114)); }
    public void Cancel() { StateText = "STOP"; IsRunning = false; }
}

public abstract class Bindable : INotifyPropertyChanged
{
    public event PropertyChangedEventHandler? PropertyChanged;
    protected void Set<T>(ref T field, T value, [CallerMemberName] string? property = null)
    {
        if (Equals(field, value)) return;
        field = value;
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(property));
    }
}
