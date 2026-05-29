namespace ArchiveToolUI
{
    /// <summary>
    /// 右下角带小三角标记的按钮，提示用户可以右键展开菜单（类似 PS 工具栏）
    /// </summary>
    public class DropdownButton : Button
    {
        private const int TriangleSize = 7;

        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);

            // 在右下角画直角三角形
            var g = e.Graphics;
            int x = Width  - TriangleSize - 2;
            int y = Height - TriangleSize - 2;

            var pts = new System.Drawing.Point[] {
                new(x + TriangleSize, y),                    // 右上角
                new(x + TriangleSize, y + TriangleSize),     // 右下角（直角）
                new(x,                y + TriangleSize),     // 左下角
            };

            using var brush = new System.Drawing.SolidBrush(
                Enabled ? System.Drawing.Color.FromArgb(100, 80, 80, 80)
                        : System.Drawing.Color.FromArgb(60, 128, 128, 128));
            g.FillPolygon(brush, pts);
        }
    }
}
