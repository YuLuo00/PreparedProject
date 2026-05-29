namespace ArchiveToolUI
{
    /// <summary>简单的密码输入对话框</summary>
    public class AddPasswordDialog : Form
    {
        public string Password => txtPwd.Text.Trim();

        private readonly TextBox txtPwd    = new();
        private readonly Button  btnOk     = new();
        private readonly Button  btnCancel = new();
        private readonly Label   lblHint   = new();

        public AddPasswordDialog(string defaultValue = "")
        {
            this.Text            = "添加密码";
            this.ClientSize      = new Size(300, 100);
            this.FormBorderStyle = FormBorderStyle.FixedDialog;
            this.MaximizeBox     = false;
            this.MinimizeBox     = false;
            this.StartPosition   = FormStartPosition.CenterParent;

            lblHint.Text     = "输入要添加到密码本的密码：";
            lblHint.Location = new Point(8, 10);
            lblHint.Size     = new Size(280, 18);

            txtPwd.Text     = defaultValue;
            txtPwd.Location = new Point(8, 32);
            txtPwd.Size     = new Size(280, 23);
            txtPwd.SelectAll();

            btnOk.Text           = "添加";
            btnOk.Location       = new Point(130, 64);
            btnOk.Size           = new Size(70, 26);
            btnOk.DialogResult   = DialogResult.OK;
            btnOk.Font           = new Font(btnOk.Font, FontStyle.Bold);

            btnCancel.Text         = "取消";
            btnCancel.Location     = new Point(210, 64);
            btnCancel.Size         = new Size(70, 26);
            btnCancel.DialogResult = DialogResult.Cancel;

            this.AcceptButton = btnOk;
            this.CancelButton = btnCancel;

            this.Controls.AddRange(new Control[] { lblHint, txtPwd, btnOk, btnCancel });
        }
    }
}
