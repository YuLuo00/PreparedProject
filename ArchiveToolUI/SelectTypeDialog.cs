namespace ArchiveToolUI
{
    /// <summary>批量模式：为单个文件选择压缩类型的对话框</summary>
    public class SelectTypeDialog : Form
    {
        public string SelectedType => cmbType.Text.Trim();

        private readonly ComboBox cmbType   = new();
        private readonly Button   btnOk     = new();
        private readonly Button   btnClear  = new();
        private readonly Button   btnCancel = new();
        private readonly Label    lblHint   = new();

        public SelectTypeDialog(string currentType, IEnumerable<string> availableTypes)
        {
            this.Text            = "设置压缩类型";
            this.ClientSize      = new Size(300, 100);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox     = false;
            this.MinimizeBox     = false;
            this.StartPosition   = FormStartPosition.CenterParent;

            lblHint.Text     = "选择或输入该文件的压缩类型：";
            lblHint.Location = new Point(8, 10);
            lblHint.Size     = new Size(280, 18);

            cmbType.Location = new Point(8, 32);
            cmbType.Size     = new Size(280, 23);
            cmbType.Items.Add("");
            foreach (var t in availableTypes) cmbType.Items.Add(t);
            cmbType.Text = currentType;

            btnClear.Text     = "清除";
            btnClear.Location = new Point(8, 64);
            btnClear.Size     = new Size(60, 26);
            btnClear.Click   += (s, e) => cmbType.Text = "";

            btnOk.Text         = "确定";
            btnOk.Location     = new Point(150, 64);
            btnOk.Size         = new Size(65, 26);
            btnOk.DialogResult = DialogResult.OK;
            btnOk.Font         = new Font(btnOk.Font, FontStyle.Bold);

            btnCancel.Text         = "取消";
            btnCancel.Location     = new Point(222, 64);
            btnCancel.Size         = new Size(65, 26);
            btnCancel.DialogResult = DialogResult.Cancel;

            this.AcceptButton = btnOk;
            this.CancelButton = btnCancel;

            this.Controls.AddRange(new Control[] { lblHint, cmbType, btnClear, btnOk, btnCancel });
        }
    }
}
