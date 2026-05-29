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

        // 批量模式：每个文件对应一个独立的 ArchiveToolService
        private readonly List<ArchiveToolService> _batchServices = new();
        private int _batchTotal = 0;
        private int _batchDone  = 0;

        public Form1()
        {
            InitializeComponent();
            WireEvents();
            LoadArchiveTypes();
        }

        // -----------------------------------------------------------------------
        // 初始化
        // -----------------------------------------------------------------------
        private void WireEvents()
        {
            // ── 单文件：拖拽 ──────────────────────────────────────────
            txtFilePath.DragEnter += OnDragEnterSingle;
            txtFilePath.DragDrop  += OnDragDropSingle;
            txtFilePath.Leave     += (s, e) => {
                if (!string.IsNullOrWhiteSpace(txtFilePath.Text))
                    DetectType(txtFilePath.Text);
            };

            // ── 批量：拖拽多个文件 ────────────────────────────────────
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

            // ── 批量模式切换 ──────────────────────────────────────────
            chkBatchMode.CheckedChanged += (s, e) => SwitchMode(chkBatchMode.Checked);

            // ── 开始/停止 ─────────────────────────────────────────────
            btnStart.Click += BtnStart_Click;

            // ── 拷贝密码（单文件模式）────────────────────────────────
            btnCopy.Click += (s, e) => {
                if (!string.IsNullOrEmpty(txtResult.Text))
                    Clipboard.SetText(txtResult.Text);
            };

            // ── 批量结果列表：双击拷贝密码 ────────────────────────────
            lvResults.DoubleClick += (s, e) => {
                if (lvResults.SelectedItems.Count > 0) {
                    var pwd = lvResults.SelectedItems[0].SubItems[1].Text;
                    if (!string.IsNullOrEmpty(pwd)) Clipboard.SetText(pwd);
                }
            };

            // ── 批量结果列表：点击列头排序 ────────────────────────────
            lvResults.ListViewItemSorter = _lvSorter;
            lvResults.ColumnClick += (s, e) => {
                if (_lvSorter.SortColumn == e.Column)
                    _lvSorter.Ascending = !_lvSorter.Ascending;
                else {
                    _lvSorter.SortColumn = e.Column;
                    _lvSorter.Ascending  = true;
                }
                lvResults.Sort();
                // 更新列头箭头提示
                foreach (ColumnHeader col in lvResults.Columns)
                    col.Text = col.Text.TrimEnd(' ', '▲', '▼');
                var activeCol = lvResults.Columns[e.Column];
                activeCol.Text += _lvSorter.Ascending ? " ▲" : " ▼";
            };

            // ── 单文件业务层回调 ──────────────────────────────────────
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

        // -----------------------------------------------------------------------
        // 拖拽辅助
        // -----------------------------------------------------------------------
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
                DetectType(files[0]);
            }
        }

        // -----------------------------------------------------------------------
        // 模式切换
        // -----------------------------------------------------------------------
        private void SwitchMode(bool batch)
        {
            // 单文件控件
            txtFilePath.Visible = !batch;
            cmbType.Visible     = !batch;
            txtResult.Visible   = !batch;
            btnCopy.Visible     = !batch;

            // 批量控件
            txtFilePaths.Visible = batch;
            lvResults.Visible    = batch;

            if (batch) {
                // 批量模式：扩大窗口，调整控件位置
                ClientSize = new Size(400, 320);
                chkFindAll.Location  = new Point(8, 96);
                chkBatchMode.Location = new Point(180, 96);
                btnStart.Location    = new Point(8, 248);
                progressBar.Location = new Point(8, 284);
                lblStatus.Location   = new Point(292, 284);
            } else {
                // 单文件模式：恢复原始尺寸
                ClientSize = new Size(400, 180);
                chkFindAll.Location  = new Point(8, 40);
                chkBatchMode.Location = new Point(180, 40);
                btnStart.Location    = new Point(8, 94);
                progressBar.Location = new Point(8, 130);
                lblStatus.Location   = new Point(292, 130);
            }
        }

        // -----------------------------------------------------------------------
        // 检测文件类型（单文件模式）
        // -----------------------------------------------------------------------
        private void DetectType(string filePath)
        {
            _service.DetectFileType(filePath);
        }

        // -----------------------------------------------------------------------
        // 开始/停止检索
        // -----------------------------------------------------------------------
        private void BtnStart_Click(object? sender, EventArgs e)
        {
            if (_isSearching) {
                StopAll();
                return;
            }

            if (chkBatchMode.Checked)
                StartBatch();
            else
                StartSingle();
        }

        // ── 单文件模式 ────────────────────────────────────────────────
        private void StartSingle()
        {
            string filePath = txtFilePath.Text.Trim();
            if (string.IsNullOrEmpty(filePath)) {
                MessageBox.Show("请先输入或拖入文件路径", "提示"); return;
            }
            txtResult.Text    = "";
            progressBar.Value = 0;
            lblStatus.Text    = "检索中…";
            SetSearching(true);
            _service.StartPasswordSearch(filePath, chkFindAll.Checked);
        }

        // ── 批量模式 ──────────────────────────────────────────────────
        private void StartBatch()
        {
            var lines = txtFilePaths.Text
                .Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
                .Select(l => l.Trim())
                .Where(l => !string.IsNullOrEmpty(l))
                .ToList();

            if (lines.Count == 0) {
                MessageBox.Show("请输入或拖入文件路径", "提示"); return;
            }

            lvResults.Items.Clear();
            _batchServices.Clear();
            _batchTotal = lines.Count;
            _batchDone  = 0;
            progressBar.Value = 0;
            progressBar.Maximum = lines.Count;
            lblStatus.Text = $"0/{lines.Count}";
            SetSearching(true);

            foreach (var filePath in lines) {
                // 每个文件一个独立的 service
                var svc = new ArchiveToolService();
                var fp  = filePath;

                // 先在列表中添加占位行
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
            lblStatus.Text = "已停止";
        }

        // -----------------------------------------------------------------------
        // 进度更新
        // -----------------------------------------------------------------------
        private void UpdateProgressSingle(PasswordSearchProgress p)
        {
            if (p.Total > 0)
                progressBar.Value = Math.Min(100, p.Current * 100 / p.Total);
            lblStatus.Text = $"{p.Current}/{p.Total}";

            if (p.Found && string.IsNullOrEmpty(txtResult.Text))
                txtResult.Text = p.Password;

            if (p.Finished) {
                SetSearching(false);
                lblStatus.Text = string.IsNullOrEmpty(txtResult.Text)
                    ? "未找到匹配密码"
                    : $"完成 ✓ {p.Current}/{p.Total}";
            }
        }

        private void UpdateProgressBatch(string filePath, PasswordSearchProgress p)
        {
            // 找到对应的列表行
            foreach (ListViewItem item in lvResults.Items) {
                if (item.Tag as string == filePath) {
                    if (p.Found)
                        item.SubItems[1].Text = p.Password;
                    if (p.Finished) {
                        if (item.SubItems[1].Text == "检索中…")
                            item.SubItems[1].Text = "未找到";
                        _batchDone++;
                        progressBar.Value = _batchDone;
                        lblStatus.Text = $"{_batchDone}/{_batchTotal}";
                        if (_batchDone >= _batchTotal) {
                            SetSearching(false);
                            lblStatus.Text = $"完成 {_batchTotal} 个文件";
                        }
                    }
                    break;
                }
            }
        }

        // -----------------------------------------------------------------------
        // 切换检索状态
        // -----------------------------------------------------------------------
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
