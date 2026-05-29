using System.IO;

namespace ArchiveToolUI
{
    // -----------------------------------------------------------------------
    // ListView 列排序比较器
    // -----------------------------------------------------------------------
    class ListViewColumnSorter : System.Collections.IComparer
    {
        public int  SortColumn { get; set; } = 0;
        public bool Ascending  { get; set; } = true;

        public int Compare(object? x, object? y)
        {
            var lx = (ListViewItem)x!;
            var ly = (ListViewItem)y!;
            string sx = lx.SubItems.Count > SortColumn ? lx.SubItems[SortColumn].Text : "";
            string sy = ly.SubItems.Count > SortColumn ? ly.SubItems[SortColumn].Text : "";
            int result = string.Compare(sx, sy, StringComparison.CurrentCultureIgnoreCase);
            return Ascending ? result : -result;
        }
    }

    public partial class Form1 : Form
    {
        private readonly ArchiveToolService _service = new();
        private bool _isSearching = false;
        private readonly ListViewColumnSorter _lvSorter = new();

        private readonly List<ArchiveToolService> _batchServices = new();
        private int _batchTotal = 0;
        private int _batchDone  = 0;

        public Form1()
        {
            InitializeComponent();
            WireEvents();
            LoadArchiveTypes();
        }

        private void WireEvents()
        {
            txtFilePath.PlaceholderText  = "拖入文件或输入路径…";
            txtResult.PlaceholderText    = "匹配到的密码";
            txtFilePaths.PlaceholderText = "拖入多个文件，或每行输入一个路径…";

            txtFilePath.DragEnter += OnDragEnterSingle;
            txtFilePath.DragDrop  += OnDragDropSingle;
            txtFilePath.Leave     += (s, e) => {
                if (!string.IsNullOrWhiteSpace(txtFilePath.Text))
                    _service.DetectFileType(txtFilePath.Text);
            };

            txtFilePaths.DragEnter += OnDragEnterSingle;
            txtFilePaths.DragDrop  += (s, e) => {
                var files = e.Data?.GetData(DataFormats.FileDrop) as string[];
                if (files?.Length > 0) {
                    var existing = txtFilePaths.Text.Trim();
                    var newPaths = string.Join(Environment.NewLine, files);
                    txtFilePaths.Text = string.IsNullOrEmpty(existing)
                        ? newPaths
                        : existing + Environment.NewLine + newPaths;
                }
            };

            chkBatchMode.CheckedChanged += (s, e) => SwitchMode(chkBatchMode.Checked);
            btnStart.Click += BtnStart_Click;

            btnCopy.Click += (s, e) => {
                if (!string.IsNullOrEmpty(txtResult.Text))
                    Clipboard.SetText(txtResult.Text);
            };

            lvResults.DoubleClick += (s, e) => {
                if (lvResults.SelectedItems.Count > 0) {
                    var pwd = lvResults.SelectedItems[0].SubItems[1].Text;
                    if (!string.IsNullOrEmpty(pwd)) Clipboard.SetText(pwd);
                }
            };

            lvResults.ListViewItemSorter = _lvSorter;
            lvResults.ColumnClick += (s, e) => {
                if (_lvSorter.SortColumn == e.Column)
                    _lvSorter.Ascending = !_lvSorter.Ascending;
                else {
                    _lvSorter.SortColumn = e.Column;
                    _lvSorter.Ascending  = true;
                }
                lvResults.Sort();
                foreach (ColumnHeader col in lvResults.Columns)
                    col.Text = col.Text.TrimEnd(' ', '▲', '▼');
                lvResults.Columns[e.Column].Text += _lvSorter.Ascending ? " ▲" : " ▼";
            };

            _service.OnTypeDetected += type => Invoke(() => {
                int idx = cmbType.Items.IndexOf(type);
                if (idx >= 0) cmbType.SelectedIndex = idx;
            });
            _service.OnPasswordProgress += p => Invoke(() => UpdateProgressSingle(p));
        }

        private void LoadArchiveTypes()
        {
            var keys = new List<string>();
            try { ArchiveToolNative.GetKeys((key, _) => keys.Add(key), IntPtr.Zero); } catch { }
            cmbType.Items.Clear();
            cmbType.Items.Add("Auto");
            foreach (var k in keys) cmbType.Items.Add(k);
            cmbType.SelectedIndex = 0;
        }

        private static void OnDragEnterSingle(object? s, DragEventArgs e)
        {
            if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
                e.Effect = DragDropEffects.Copy;
        }

        private void OnDragDropSingle(object? s, DragEventArgs e)
        {
            var files = e.Data?.GetData(DataFormats.FileDrop) as string[];
            if (files?.Length > 0) {
                txtFilePath.Text = files[0];
                _service.DetectFileType(files[0]);
            }
        }

        // ── 模式切换 ──────────────────────────────────────────────────
        private void SwitchMode(bool batch)
        {
            txtFilePath.Visible  = !batch;
            cmbType.Visible      = !batch;
            txtResult.Visible    = !batch;
            btnCopy.Visible      = !batch;
            txtFilePaths.Visible = batch;
            lvResults.Visible    = batch;

            if (batch) {
                ClientSize = new Size(400, 298);
                chkFindAll.Location   = new Point(8, 96);
                chkBatchMode.Location = new Point(180, 96);
                btnStart.Location     = new Point(8, 222);
            } else {
                ClientSize = new Size(400, 158);
                chkFindAll.Location   = new Point(8, 40);
                chkBatchMode.Location = new Point(180, 40);
                btnStart.Location     = new Point(8, 94);
            }
        }

        // ── 开始/停止 ─────────────────────────────────────────────────
        private void BtnStart_Click(object? sender, EventArgs e)
        {
            if (_isSearching) { StopAll(); return; }
            if (chkBatchMode.Checked) StartBatch(); else StartSingle();
        }

        private void StartSingle()
        {
            string fp = txtFilePath.Text.Trim();
            if (string.IsNullOrEmpty(fp)) { MessageBox.Show("请先输入或拖入文件路径", "提示"); return; }
            txtResult.Text         = "";
            tsProgressBar.Value    = 0;
            tsLabel.Text           = "检索中…";
            SetSearching(true);
            _service.StartPasswordSearch(fp, chkFindAll.Checked);
        }

        private void StartBatch()
        {
            var lines = txtFilePaths.Text
                .Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
                .Select(l => l.Trim()).Where(l => !string.IsNullOrEmpty(l)).ToList();
            if (lines.Count == 0) { MessageBox.Show("请输入或拖入文件路径", "提示"); return; }

            lvResults.Items.Clear();
            _batchServices.Clear();
            _batchTotal = lines.Count;
            _batchDone  = 0;
            tsProgressBar.Value   = 0;
            tsProgressBar.Maximum = lines.Count;
            tsLabel.Text          = $"0/{lines.Count}";
            SetSearching(true);

            foreach (var fp in lines) {
                var svc  = new ArchiveToolService();
                var item = new ListViewItem(Path.GetFileName(fp));
                item.SubItems.Add("检索中…");
                item.Tag = fp;
                lvResults.Items.Add(item);
                svc.OnPasswordProgress += p => Invoke(() => UpdateProgressBatch(fp, p));
                _batchServices.Add(svc);
                svc.StartPasswordSearch(fp, chkFindAll.Checked);
            }
        }

        private void StopAll()
        {
            _service.CancelSearch();
            foreach (var svc in _batchServices) svc.CancelSearch();
            SetSearching(false);
            tsLabel.Text = "已停止";
        }

        // ── 进度更新 ──────────────────────────────────────────────────
        private void UpdateProgressSingle(PasswordSearchProgress p)
        {
            if (p.Total > 0)
                tsProgressBar.Value = Math.Min(100, p.Current * 100 / p.Total);
            tsLabel.Text = $"{p.Current}/{p.Total}";
            if (p.Found && string.IsNullOrEmpty(txtResult.Text))
                txtResult.Text = p.Password;
            if (p.Finished) {
                SetSearching(false);
                tsLabel.Text = string.IsNullOrEmpty(txtResult.Text)
                    ? "未找到匹配密码"
                    : $"完成 ✓ {p.Current}/{p.Total}";
            }
        }

        private void UpdateProgressBatch(string filePath, PasswordSearchProgress p)
        {
            foreach (ListViewItem item in lvResults.Items) {
                if (item.Tag as string == filePath) {
                    if (p.Found) item.SubItems[1].Text = p.Password;
                    if (p.Finished) {
                        if (item.SubItems[1].Text == "检索中…") item.SubItems[1].Text = "未找到";
                        _batchDone++;
                        tsProgressBar.Value = _batchDone;
                        tsLabel.Text = $"{_batchDone}/{_batchTotal}";
                        if (_batchDone >= _batchTotal) {
                            SetSearching(false);
                            tsLabel.Text = $"完成 {_batchTotal} 个文件";
                        }
                    }
                    break;
                }
            }
        }

        private void SetSearching(bool searching)
        {
            _isSearching         = searching;
            txtFilePath.Enabled  = !searching;
            txtFilePaths.Enabled = !searching;
            cmbType.Enabled      = !searching;
            chkFindAll.Enabled   = !searching;
            chkBatchMode.Enabled = !searching;
            btnStart.Text        = searching ? "停止检索" : "开始检索";
            btnStart.BackColor   = searching ? Color.IndianRed : SystemColors.Control;
            btnStart.ForeColor   = searching ? Color.White : SystemColors.ControlText;
        }
    }
}
