using System.Runtime.InteropServices;

namespace RawMoveUI
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

        }

        private void Bt_raw2store_click(object sender, EventArgs e)
        {
            Program.clr.MoveAllRawToStorePath();
        }

        void SyncThePath()
        {
            Program.clr.SetRootDir(this.u_tb_rootDirPath.Text);
            Program.clr.SetRawStoreDir(this.u_tb_rawStorePath.Text);
        }
        void ErrMsgBack(IntPtr msg, IntPtr obj, int level)
        {
            string? message = Marshal.PtrToStringUni(msg);
            Console.WriteLine(message);
        }
        void test()
        {
            NS_RawMoveCLR.RawMoveCLR.CallbackDelegate dele = this.ErrMsgBack;
            Program.clr.SetCbErrMsg(dele);
        }
        /// <summary>
        /// 双击
        /// 选一个文件夹作为root dir
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void RootDirPath_DoubleClick(object sender, EventArgs e)
        {
            folderBrowserDialog1.Description = "请选择一个文件夹";
            folderBrowserDialog1.SelectedPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop); // 默认打开路径
            if(Directory.Exists(this.u_tb_rootDirPath.Text)) {
                folderBrowserDialog1.SelectedPath = this.u_tb_rootDirPath.Text;
            }

            if(folderBrowserDialog1.ShowDialog() == DialogResult.OK) {
                string folderPath = folderBrowserDialog1.SelectedPath;
                this.u_tb_rootDirPath.Text = folderPath;
                Console.WriteLine($"选择的文件夹: {folderPath}");
            }
            else {
                Console.WriteLine("未选择文件夹");
            }
        }

        /// <summary>
        /// 双击
        /// 选一个文件夹作为raw store
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void Tb_rawStorePath_DoubleClick(object sender, EventArgs e)
        {
            folderBrowserDialog1.Description = "请选择一个文件夹";
            folderBrowserDialog1.SelectedPath = Environment.GetFolderPath(Environment.SpecialFolder.Desktop); // 默认打开路径
            if(Directory.Exists(this.u_tb_rawStorePath.Text)) {
                folderBrowserDialog1.SelectedPath = this.u_tb_rawStorePath.Text;
            }
            else if(Directory.Exists(this.u_tb_rootDirPath.Text)) {
                folderBrowserDialog1.SelectedPath = this.u_tb_rootDirPath.Text;
            }

            if(folderBrowserDialog1.ShowDialog() == DialogResult.OK) {
                string folderPath = folderBrowserDialog1.SelectedPath;
                this.u_tb_rawStorePath.Text = folderPath;
                Console.WriteLine($"选择的文件夹: {folderPath}");
            }
            else {
                Console.WriteLine("未选择文件夹");
            }
        }

        private void Form1_Load(object sender, EventArgs e)
        {

        }

        private void u_tb_rootDirPath_TextChanged(object sender, EventArgs e)
        {
            Program.clr.SetRootDir(this.u_tb_rootDirPath.Text);
        }

        private void u_tb_rawStorePath_TextChanged(object sender, EventArgs e)
        {
            Program.clr.SetRawStoreDir(this.u_tb_rawStorePath.Text);
        }

        private void dataGridView1_CellContentClick(object sender, DataGridViewCellEventArgs e)
        {

        }
    }
}
