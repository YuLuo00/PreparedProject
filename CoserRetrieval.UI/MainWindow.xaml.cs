using System;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.IO;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace CoserRetrieval.UI;

public partial class MainWindow : Window
{
    private readonly DispatcherTimer _timer = new() { Interval = TimeSpan.FromSeconds(1) };
    private int _tick;

    public ObservableCollection<TaskRow> Tasks { get; } = new();
    public string NativeStatus { get; private set; }

    public MainWindow()
    {
        InitializeComponent();
        NativeStatus = ReadNativeStatus();
        CreateTestTasks();
        DataContext = this;
        _timer.Tick += (_, _) => AdvanceTestNotifications();
        _timer.Start();
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
        Tasks.Add(new TaskRow(Path.Combine(images, "rioko_ref.jpg"), DemoOutcome.Success));
        Tasks.Add(new TaskRow(Path.Combine(images, "chichi_query.jpg"), DemoOutcome.MiddleFailure));
        Tasks.Add(new TaskRow(Path.Combine(images, "illustration_reject.jpg"), DemoOutcome.TotalFailure));
    }

    private void AdvanceTestNotifications()
    {
        _tick++;
        foreach (TaskRow task in Tasks) task.Advance(_tick);
        if (_tick >= 7) _timer.Stop();
    }

    private static string FindRegressionDirectory()
    {
        DirectoryInfo? directory = new(AppContext.BaseDirectory);
        while (directory is not null)
        {
            string candidate = Path.Combine(directory.FullName, "CoserRetrieval", "testdata", "regression");
            if (Directory.Exists(candidate)) return candidate;
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
    public void Complete() { StateText = "DONE"; IsRunning = false; Background = new SolidColorBrush(Color.FromRgb(30, 67, 51)); BorderBrush = new SolidColorBrush(Color.FromRgb(69, 143, 98)); Foreground = new SolidColorBrush(Color.FromRgb(105, 223, 145)); }
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
