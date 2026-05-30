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
        private CancellationTokenSource? _batchCts;
        private int _batchTotal = 0;
        private int _batchDone  = 0;
        private readonly List<string> _availableTypes = new();

        public Form1()
        {
            InitializeComponent();
            WireEvents();
            LoadArchiveTypes();
            StartBackgroundInit();
        }

        // ── 后台初始化 7-Zip DLL ──────────────────────────────────────
        private void StartBackgroundInit()
        {
            btnStart.Enabled = false;
            toolTip.SetToolTip(btnStart, "正在初始化 7-Zip 库，请稍候…");
            tsLabel.Text = "正在初始化 7-Zip…";

            Task.Run(() => {
                ArchiveToolNative.InitArchiveTool();
                Invoke(() => {
                    btnStart.Enabled = true;
                    toolTip.SetToolTip(btnStart, "");
                    tsLabel.Text = "就绪";
                });
            });
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

            // 浏览按钮：选一个文件保持单文件模式，选多个自动切换批量模式
            btnBrowse.Click += (s, e) => {
                using var dlg = new OpenFileDialog {
                    Title       = "选择压缩文件",
                    Multiselect = true,
                    Filter      = "压缩文件|*.zip;*.rar;*.7z;*.tar;*.gz;*.bz2|所有文件|*.*",
                };
                if (dlg.ShowDialog(this) != DialogResult.OK) return;

                if (dlg.FileNames.Length == 1) {
                    // 单文件：保持单文件模式
                    if (chkBatchMode.Checked) {
                        chkBatchMode.Checked = false;
                        SwitchMode(false);
                    }
                    txtFilePath.Text = dlg.FileNames[0];
                    _service.DetectFileType(dlg.FileNames[0]);
                } else {
                    // 多文件：自动切换到批量模式
                    if (!chkBatchMode.Checked) {
                        chkBatchMode.Checked = true;
                        SwitchMode(true);
                    }
                    txtFilePaths.Text = string.Join(Environment.NewLine, dlg.FileNames);
                }
            };

            chkBatchMode.CheckedChanged += (s, e) => SwitchMode(chkBatchMode.Checked);
            this.Resize += (s, e) => AdjustBatchLayout();
            btnStart.Click += BtnStart_Click;

            btnCopy.Click += (s, e) => {
                if (!string.IsNullOrEmpty(txtResult.Text))
                    Clipboard.SetText(txtResult.Text);
            };

            // 测试按钮事件：输入密码后遍历所有类型尝试解压并展示结果
            btnTestAllTypes.Click += async (s, e) => {
                string fp = txtFilePath.Text.Trim();
                if (string.IsNullOrEmpty(fp)) { MessageBox.Show("请先输入或拖入文件路径", "提示"); return; }
                using var dlg = new AddPasswordDialog("");
                if (dlg.ShowDialog(this) != DialogResult.OK) return;
                string pwd = dlg.Password;
                btnTestAllTypes.Enabled = false;
                tsLabel.Text = "测试中…";
                try {
                    string res = await _service.TestPasswordAllTypesAsync(fp, pwd);
                    using var win = new Form { Text = "测试结果", Width = 600, Height = 400, StartPosition = FormStartPosition.CenterParent };
                    var txt = new TextBox { Multiline = true, ReadOnly = true, Dock = DockStyle.Fill, ScrollBars = ScrollBars.Vertical, Text = res };
                    win.Controls.Add(txt);
                    win.ShowDialog(this);
                } catch (Exception ex) {
                    MessageBox.Show($"测试失败: {ex.Message}", "错误");
                } finally {
                    btnTestAllTypes.Enabled = true;
                    tsLabel.Text = "就绪";
                }
            };

            // 右键"拷贝密码"→ 添加密码到密码本
            menuAddPwd.Click += (s, e) => {
                // 默认填入结果框第一行（或空）
                string defaultPwd = txtResult.Text.Contains(Environment.NewLine)
                    ? txtResult.Text.Split(Environment.NewLine)[0].Trim()
                    : txtResult.Text.Trim();

                using var dlg = new AddPasswordDialog(defaultPwd);
                if (dlg.ShowDialog(this) == DialogResult.OK && !string.IsNullOrEmpty(dlg.Password)) {
                    int ok = ArchiveToolNative.AddNewPwd(dlg.Password);
                    tsLabel.Text = ok != 0 ? $"已添加: {dlg.Password}" : "添加失败（已存在？）";
                }
            };

            lvResults.DoubleClick += (s, e) => {
                if (lvResults.SelectedItems.Count == 0) return;
                var item = lvResults.SelectedItems[0];

                // 获取鼠标点击位置，判断点击的是哪一列
                var pt = lvResults.PointToClient(Cursor.Position);
                var hitInfo = lvResults.HitTest(pt);
                int col = hitInfo.Item != null ? hitInfo.SubItem != null
                    ? item.SubItems.IndexOf(hitInfo.SubItem) : 0 : 0;

                if (col == 2) {
                    // 双击"类型"列 → 弹出类型选择对话框
                    using var dlg = new SelectTypeDialog(item.SubItems[2].Text, _availableTypes);
                    if (dlg.ShowDialog(this) == DialogResult.OK)
                        item.SubItems[2].Text = dlg.SelectedType;
                } else {
                    // 双击其他列 → 拷贝密码
                    var pwd = item.SubItems[1].Text;
                    if (!string.IsNullOrEmpty(pwd) && pwd != "检索中…" && pwd != "未找到")
                        Clipboard.SetText(pwd);
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

            _service.OnRawFormatDetected += raw => Invoke(() => {
                // 状态栏显示 libarchive 原始格式字符串，方便用户做 json 矫正
                tsTypeLabel.Text = string.IsNullOrEmpty(raw) ? "" : $"原始: {raw}";
            });

            // 点击原始格式标签 → 复制到剪贴板
            tsTypeLabel.Click += (s, e) => {
                if (string.IsNullOrEmpty(tsTypeLabel.Text)) return;
                // 去掉 "原始: " 前缀
                string raw = tsTypeLabel.Text.StartsWith("原始: ")
                    ? tsTypeLabel.Text[4..] : tsTypeLabel.Text;
                Clipboard.SetText(raw);
                string orig = tsTypeLabel.Text;
                tsTypeLabel.Text = "已复制 ✓";
                Task.Delay(1500).ContinueWith(_ => Invoke(() => tsTypeLabel.Text = orig));
            };
            tsTypeLabel.IsLink = true;
            toolTip.SetToolTip(statusStrip, "点击原始格式标签可复制到剪贴板");

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
            _availableTypes.Clear();
            _availableTypes.Add("Auto");
            foreach (var k in keys) { cmbType.Items.Add(k); _availableTypes.Add(k); }
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
            btnBrowse.Visible    = !batch;
            txtFilePath.Visible  = !batch;
            cmbType.Visible      = !batch;
            txtResult.Visible    = !batch;
            btnCopy.Visible      = !batch;
            txtFilePaths.Visible = batch;
            lvResults.Visible    = batch;

            if (batch) {
                MinimumSize = new Size(320, 320);
                if (ClientSize.Height < 298) ClientSize = new Size(Math.Max(ClientSize.Width, 400), 298);
                chkFindAll.Location   = new Point(8, 96);
                chkBatchMode.Location = new Point(180, 96);
                lvResults.Location    = new Point(8, 122);
                AdjustBatchLayout();
            } else {
                MinimumSize = new Size(320, 190);
                if (ClientSize.Height > 200) ClientSize = new Size(ClientSize.Width, 158);
                chkFindAll.Location   = new Point(8, 40);
                chkBatchMode.Location = new Point(180, 40);
            }
        }

        // lvResults 高度自适应：填充 chkBatchMode 和 btnStart 之间的空间
        private void AdjustBatchLayout()
        {
            if (!chkBatchMode.Checked || !lvResults.Visible) return;
            int top    = lvResults.Location.Y;
            int bottom = btnStart.Location.Y - 4;
            int height = Math.Max(40, bottom - top);
            int width  = ClientSize.Width - 16;
            lvResults.Size = new Size(width, height);
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

            // 先用空密码测试，判断文件是否有密码
            string type = cmbType.SelectedItem?.ToString() ?? "Auto";
            if (ArchiveToolNative.ArchiveExtraTest(fp, "", type) != 0) {
                txtResult.Text             = "";
                txtResult.PlaceholderText  = "✓ 无需密码，可直接解压";
                tsLabel.Text               = "无需密码";
                return;
            }
            txtResult.PlaceholderText = "匹配到的密码";

            txtResult.Text         = "";
            tsProgressBar.Value    = 0;
            tsLabel.Text           = "检索中…";
            SetSearching(true);
            _service.StartPasswordSearch(fp, chkFindAll.Checked, type);
        }

        private void StartBatch()
        {
            var lines = txtFilePaths.Text
                .Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries)
                .Select(l => l.Trim()).Where(l => !string.IsNullOrEmpty(l)).ToList();
            if (lines.Count == 0) { MessageBox.Show("请输入或拖入文件路径", "提示"); return; }

            // 如果 ListView 已有条目（用户预设了 type），保留 type；否则重建列表
            bool hasExisting = lvResults.Items.Count == lines.Count;

            if (!hasExisting) {
                lvResults.Items.Clear();
                foreach (var fp in lines) {
                    var item = new ListViewItem(Path.GetFileName(fp));
                    item.SubItems.Add("检索中…");
                    item.SubItems.Add("");   // 类型列
                    item.SubItems.Add("");   // 原始格式列
                    item.Tag = fp;
                    lvResults.Items.Add(item);
                }
            } else {
                // 重置密码列，保留类型列
                foreach (ListViewItem item in lvResults.Items)
                    item.SubItems[1].Text = "检索中…";
            }

            _batchServices.Clear();
            _batchTotal = lines.Count;
            _batchDone  = 0;
            tsProgressBar.Value   = 0;
            tsProgressBar.Maximum = lines.Count;
            tsLabel.Text          = $"0/{lines.Count}";
            SetSearching(true);

            _batchCts?.Cancel();
            _batchCts = new CancellationTokenSource();
            _ = RunBatchSerialAsync(_batchCts.Token);
            return;

#if false
            foreach (ListViewItem item in lvResults.Items) {
                var svc = new ArchiveToolService();
                var fp  = item.Tag as string ?? "";
                string? presetType = item.SubItems.Count >= 3 && !string.IsNullOrEmpty(item.SubItems[2].Text)
                    ? item.SubItems[2].Text : null;

                var capturedItem = item;
                Task.Run(() => {
                    // 获取原始格式
                    byte[] rawBuf = new byte[256];
                    int rawLen = ArchiveToolNative.check_format(fp, rawBuf, rawBuf.Length);
                    string raw = rawLen > 0 ? System.Text.Encoding.UTF8.GetString(rawBuf, 0, rawLen) : "";

                    // 先用空密码测试，判断是否无需密码
                    string testType = presetType ?? "Auto";
                    bool noPassword = ArchiveToolNative.ArchiveExtraTest(fp, "", testType) != 0;

                    Invoke(() => {
                        if (capturedItem.SubItems.Count >= 4)
                            capturedItem.SubItems[3].Text = raw;
                        if (noPassword) {
                            capturedItem.SubItems[1].Text = "✓ 无需密码";
                            _batchDone++;
                            tsProgressBar.Value = _batchDone;
                            tsLabel.Text = $"{_batchDone}/{_batchTotal}";
                            if (_batchDone >= _batchTotal) {
                                SetSearching(false);
                                tsLabel.Text = $"完成 {_batchTotal} 个文件";
                            }
                        }
                    });

                    if (!noPassword) {
                        svc.OnPasswordProgress += p => Invoke(() => UpdateProgressBatch(fp, p));
                        _batchServices.Add(svc);
                        svc.StartPasswordSearch(fp, chkFindAll.Checked, presetType);
                    }
                });
            }
#endif
        }

        private async Task RunBatchSerialAsync(CancellationToken token)
        {
            foreach (ListViewItem item in lvResults.Items) {
                if (token.IsCancellationRequested) break;

                var svc = new ArchiveToolService();
                var fp  = item.Tag as string ?? "";
                string? presetType = item.SubItems.Count >= 3 && !string.IsNullOrEmpty(item.SubItems[2].Text)
                    ? item.SubItems[2].Text : null;

                try {
                    var precheck = await Task.Run(() => {
                        byte[] rawBuf = new byte[256];
                        int rawLen = ArchiveToolNative.check_format(fp, rawBuf, rawBuf.Length);
                        string raw = rawLen > 0 ? System.Text.Encoding.UTF8.GetString(rawBuf, 0, rawLen) : "";

                        string testType = presetType ?? "Auto";
                        bool noPassword = ArchiveToolNative.ArchiveExtraTest(fp, "", testType) != 0;
                        return (raw, noPassword);
                    }, token);

                    if (item.SubItems.Count >= 4)
                        item.SubItems[3].Text = precheck.raw;

                    if (precheck.noPassword) {
                        item.SubItems[1].Text = "No password";
                        _batchDone++;
                        tsProgressBar.Value = _batchDone;
                        tsLabel.Text = $"{_batchDone}/{_batchTotal}";
                        continue;
                    }

                    _batchServices.Add(svc);
                    await SearchBatchItemAsync(svc, fp, presetType, token);
                }
                catch (OperationCanceledException) {
                    break;
                }
                finally {
                    _batchServices.Remove(svc);
                }
            }

            if (!token.IsCancellationRequested && _batchDone >= _batchTotal) {
                SetSearching(false);
                tsLabel.Text = $"Done {_batchTotal} files";
            }
        }

        private Task SearchBatchItemAsync(
            ArchiveToolService svc,
            string filePath,
            string? presetType,
            CancellationToken token)
        {
            var tcs = new TaskCompletionSource(TaskCreationOptions.RunContinuationsAsynchronously);
            CancellationTokenRegistration reg = default;

            void Handler(PasswordSearchProgress p)
            {
                if (IsDisposed) {
                    tcs.TrySetCanceled();
                    return;
                }

                BeginInvoke(() => {
                    UpdateProgressBatch(filePath, p);
                    if (p.Finished) {
                        tcs.TrySetResult();
                    }
                });
            }

            reg = token.Register(() => {
                svc.CancelSearch();
                tcs.TrySetCanceled(token);
            });

            svc.OnPasswordProgress += Handler;
            try {
                svc.StartPasswordSearch(filePath, chkFindAll.Checked, presetType);
            }
            catch {
                svc.OnPasswordProgress -= Handler;
                reg.Dispose();
                throw;
            }

            return tcs.Task.ContinueWith(t => {
                svc.OnPasswordProgress -= Handler;
                reg.Dispose();
                return t;
            }, CancellationToken.None, TaskContinuationOptions.ExecuteSynchronously, TaskScheduler.Default).Unwrap();
        }

        private void StopAll()
        {
            _batchCts?.Cancel();
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
            if (p.Found) {
                txtResult.Text = string.IsNullOrEmpty(txtResult.Text)
                    ? p.Password
                    : txtResult.Text + Environment.NewLine + p.Password;
            }
            if (p.Finished) {
                SetSearching(false);
                if (!string.IsNullOrEmpty(txtResult.Text)) {
                    tsLabel.Text = $"完成 ✓ {p.Current}/{p.Total}";
                    // 将第一个匹配密码提升到密码本最前面
                    string firstPwd = txtResult.Text.Split(Environment.NewLine)[0].Trim();
                    if (!string.IsNullOrEmpty(firstPwd))
                        Task.Run(() => ArchiveToolNative.PromotePwd(firstPwd));
                } else {
                    tsLabel.Text = "未找到匹配密码";
                }
            }
        }

        private void UpdateProgressBatch(string filePath, PasswordSearchProgress p)
        {
            foreach (ListViewItem item in lvResults.Items) {
                if (item.Tag as string == filePath) {
                    if (!string.IsNullOrEmpty(p.Type) && item.SubItems.Count >= 3
                        && string.IsNullOrEmpty(item.SubItems[2].Text))
                        item.SubItems[2].Text = p.Type;
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
