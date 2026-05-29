namespace ArchiveToolUI
{
    public partial class Form1 : Form
    {
        private readonly ArchiveToolService _service = new();
        private bool _isSearching = false;

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
            // 拖拽文件到地址栏
            txtFilePath.DragEnter += (s, e) => {
                if (e.Data?.GetDataPresent(DataFormats.FileDrop) == true)
                    e.Effect = DragDropEffects.Copy;
            };
            txtFilePath.DragDrop += (s, e) => {
                var files = e.Data?.GetData(DataFormats.FileDrop) as string[];
                if (files?.Length > 0) {
                    txtFilePath.Text = files[0];
                    DetectType(files[0]);
                }
            };

            // 地址栏失焦时自动检测类型
            txtFilePath.Leave += (s, e) => {
                if (!string.IsNullOrWhiteSpace(txtFilePath.Text))
                    DetectType(txtFilePath.Text);
            };

            // 开始/停止按钮
            btnStart.Click += BtnStart_Click;

            // 拷贝密码
            btnCopy.Click += (s, e) => {
                if (!string.IsNullOrEmpty(txtResult.Text))
                    Clipboard.SetText(txtResult.Text);
            };

            // 业务层回调
            _service.OnTypeDetected += type => {
                Invoke(() => {
                    // 在下拉框中选中对应类型
                    int idx = cmbType.Items.IndexOf(type);
                    if (idx >= 0) cmbType.SelectedIndex = idx;
                });
            };

            _service.OnPasswordProgress += p => {
                Invoke(() => UpdateProgress(p));
            };
        }

        private void LoadArchiveTypes()
        {
            // 从 DLL 获取支持的格式列表
            var keys = new System.Collections.Generic.List<string>();
            try {
                ArchiveToolNative.GetKeys((key, _) => keys.Add(key), IntPtr.Zero);
            } catch { }

            cmbType.Items.Clear();
            cmbType.Items.Add("Auto");
            foreach (var k in keys) cmbType.Items.Add(k);
            cmbType.SelectedIndex = 0;
        }

        // -----------------------------------------------------------------------
        // 检测文件类型（后台）
        // -----------------------------------------------------------------------
        private void DetectType(string filePath)
        {
            _service.OnTypeDetected += OnTypeDetectedOnce;
            _service.DetectFileType(filePath);

            void OnTypeDetectedOnce(string type) {
                _service.OnTypeDetected -= OnTypeDetectedOnce;
            }
        }

        // -----------------------------------------------------------------------
        // 开始/停止检索
        // -----------------------------------------------------------------------
        private void BtnStart_Click(object? sender, EventArgs e)
        {
            if (_isSearching) {
                // 停止
                _service.CancelSearch();
                SetSearching(false);
                return;
            }

            string filePath = txtFilePath.Text.Trim();
            if (string.IsNullOrEmpty(filePath)) {
                MessageBox.Show("请先输入或拖入文件路径", "提示");
                return;
            }

            txtResult.Text = "";
            progressBar.Value = 0;
            lblStatus.Text = "检索中…";
            SetSearching(true);

            _service.StartPasswordSearch(filePath, chkFindAll.Checked);
        }

        // -----------------------------------------------------------------------
        // 进度更新（已在 UI 线程）
        // -----------------------------------------------------------------------
        private void UpdateProgress(PasswordSearchProgress p)
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

        // -----------------------------------------------------------------------
        // 切换检索状态
        // -----------------------------------------------------------------------
        private void SetSearching(bool searching)
        {
            _isSearching       = searching;
            txtFilePath.Enabled = !searching;
            cmbType.Enabled    = !searching;
            chkFindAll.Enabled = !searching;
            btnStart.Text      = searching ? "停止检索" : "开始检索";
            btnStart.BackColor = searching ? Color.IndianRed : SystemColors.Control;
            btnStart.ForeColor = searching ? Color.White : SystemColors.ControlText;
        }
    }
}
