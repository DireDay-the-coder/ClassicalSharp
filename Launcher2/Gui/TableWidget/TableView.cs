// Decompiled with JetBrains decompiler
// Type: Launcher.Gui.Widgets.TableView
// Assembly: Launcher2, Version=0.99.9.99, Culture=neutral, PublicKeyToken=null
// MVID: 9A5D6DDE-06CE-47A5-BCD3-CE89077CCD3C
// Assembly location: E:\Dires Dev Space\MCDire\working client - Copy\Launcher2.exe

using ClassicalSharp;
using Launcher.Drawing;
using Launcher.Web;
using System;
using System.Collections.Generic;
using System.Drawing;

namespace Launcher.Gui.Widgets
{
  public sealed class TableView
  {
    internal static PackedCol backGridCol = new PackedCol(20, 20, 10);
    internal static PackedCol foreGridCol = new PackedCol(40, 40, 40);
    private static TableView.ColumnFilter filterFlag = new TableView.ColumnFilter(TableView.FilterFlag);
    private static TableView.ColumnFilter filterName = new TableView.ColumnFilter(TableView.FilterName);
    private static TableView.ColumnFilter filterOnline = new TableView.ColumnFilter(TableView.FilterOnline);
    private static TableView.ColumnFilter filterMax = new TableView.ColumnFilter(TableView.FilterMax);
    private static TableView.ColumnFilter filterUptime = new TableView.ColumnFilter(TableView.FilterUptime);
    private static TableView.ColumnFilter filterIP = new TableView.ColumnFilter(TableView.FilterIP);
    private static TableView.ColumnFilter filterSoftware = new TableView.ColumnFilter(TableView.FilterSoftware);
    private int entryHeight;
    private int headerHeight;
    internal int headerStartY;
    internal int headerEndY;
    internal int numEntries;
    internal int maxIndex;
    private LauncherWindow game;
    private TableWidget table;
    private Font font;
    private Font titleFont;
    public const int flagPadding = 15;

    public void Init(LauncherWindow game, TableWidget table)
    {
      this.game = game;
      this.table = table;
    }

    public void SetDrawData(IDrawer2D drawer, Font font, Font titleFont)
    {
      this.font = font;
      this.titleFont = titleFont;
      this.headerHeight = drawer.FontHeight(titleFont, true);
      this.entryHeight = drawer.FontHeight(font, true);
    }

    public void RecalculateDrawData()
    {
      for (int index = 0; index < this.table.ColumnWidths.Length; ++index)
      {
        this.table.ColumnWidths[index] = this.table.DesiredColumnWidths[index];
        Utils.Clamp(ref this.table.ColumnWidths[index], 20, this.game.Width - 20);
      }
      this.table.Width = this.game.Width - this.table.X;
      this.ResetEntries();
      int num1 = this.table.Y + 3 + (this.headerHeight + 2);
      this.maxIndex = this.table.Count;
      int num2 = num1 + 5;
      for (int currentIndex = this.table.CurrentIndex; currentIndex < this.table.Count; ++currentIndex)
      {
        if (num2 + this.entryHeight > this.table.Y + this.table.Height)
        {
          this.maxIndex = currentIndex;
          break;
        }
        this.table.entries[this.table.order[currentIndex]].Y = num2;
        this.table.entries[this.table.order[currentIndex]].Height = this.entryHeight;
        num2 += this.entryHeight + 2;
      }
    }

    public void RedrawData(IDrawer2D drawer)
    {
      this.DrawGrid(drawer);
      int x1 = this.table.X + 15 + 5;
      int x2 = x1 + (this.DrawColumn(drawer, "Flag", 0, x1, TableView.filterFlag) + 5);
      int x3 = x2 + (this.DrawColumn(drawer, "Name", 1, x2, TableView.filterName) + 5);
      int x4 = x3 + (this.DrawColumn(drawer, "Online", 2, x3, TableView.filterOnline) + 5);
      int x5 = x4 + (this.DrawColumn(drawer, "Max Slots", 3, x4, TableView.filterMax) + 5);
      int x6 = x5 + (this.DrawColumn(drawer, "Uptime", 4, x5, TableView.filterUptime) + 5);
      int x7 = x6 + (this.DrawColumn(drawer, "IP Address", 5, x6, TableView.filterIP) + 5);
      int num = x7 + (this.DrawColumn(drawer, "Software", 6, x7, TableView.filterSoftware) + 5);
      this.DrawScrollbar(drawer);
      this.DrawFlags();
    }

    public void Redraw(IDrawer2D drawer)
    {
      this.RecalculateDrawData();
      this.RedrawData(drawer);
    }

    private static string FilterFlag(TableEntry e)
    {
      return e.Flag;
    }

    private static string FilterName(TableEntry e)
    {
      return e.Name;
    }

    private static string FilterOnline(TableEntry e)
    {
      return e.Online;
    }

    private static string FilterMax(TableEntry e)
    {
      return e.Max;
    }

    private static string FilterUptime(TableEntry e)
    {
      return e.Uptime;
    }

    private static string FilterIP(TableEntry e)
    {
      return e.IP;
    }

    private static string FilterSoftware(TableEntry e)
    {
      return e.Software;
    }

    private static FastBitmap GetFlag(string flag)
    {
      List<string> flags = FetchFlagsTask.Flags;
      List<FastBitmap> bitmaps = FetchFlagsTask.Bitmaps;
      for (int index = 0; index < flags.Count; ++index)
      {
        if (!(flags[index] != flag))
          return index < bitmaps.Count ? bitmaps[index] : (FastBitmap) null;
      }
      return (FastBitmap) null;
    }

    public void DrawFlags()
    {
      using (FastBitmap dst = this.game.LockBits())
      {
        for (int currentIndex = this.table.CurrentIndex; currentIndex < this.maxIndex; ++currentIndex)
        {
          TableEntry tableEntry = this.table.Get(currentIndex);
          FastBitmap flag = TableView.GetFlag(tableEntry.Flag);
          if (flag != null)
          {
            Rectangle dstRect = new Rectangle(this.table.X + 2, tableEntry.Y + 3, 16, 11);
            BitmapDrawer.Draw(flag, dst, dstRect);
          }
        }
      }
    }

    private int DrawColumn(IDrawer2D drawer, string header, int columnI, int x, TableView.ColumnFilter filter)
    {
      int y1 = this.table.Y + 3;
      int columnWidth = this.table.ColumnWidths[columnI];
      bool flag = columnI > 0;
      DrawTextArgs args = new DrawTextArgs(header, this.titleFont, true);
      TableEntry entry1 = new TableEntry();
      this.DrawColumnEntry(drawer, ref args, columnWidth, x, ref y1, ref entry1);
      this.maxIndex = this.table.Count;
      int y2 = y1 + 5;
      for (int currentIndex = this.table.CurrentIndex; currentIndex < this.table.Count; ++currentIndex)
      {
        TableEntry entry2 = this.table.Get(currentIndex);
        args = new DrawTextArgs(filter(entry2), this.font, true);
        int num = int.Parse(entry2.Players.Substring(0, entry2.Players.IndexOf('/')));
        if ((currentIndex == this.table.SelectedIndex || entry2.Featured || num == 0 || (num >= 1 && num < 3 || num >= 3 && num < 5) || (num >= 5 && num < 7 || num >= 7 && num < 10 || num >= 10)) && !flag)
        {
          int y3 = y2 - 3;
          int height = Math.Min(y3 + (this.entryHeight + 4), this.table.Y + this.table.Height) - y3;
          drawer.Clear(this.GetGridCol(entry2.Featured, currentIndex == this.table.SelectedIndex), this.table.X, y3, 65, height);
          drawer.Clear(this.GetGridColpl(num == 0, num >= 1 && num < 3, num >= 3 && num < 5, num >= 5 && num < 7, num >= 7 && num < 10, num >= 10, currentIndex == this.table.SelectedIndex), this.table.X, y3, this.table.Width, height);
        }
        if (!this.DrawColumnEntry(drawer, ref args, columnWidth, x, ref y2, ref entry2))
        {
          this.maxIndex = currentIndex;
          break;
        }
      }
      if (flag && !this.game.ClassicBackground)
        drawer.Clear(LauncherSkin.BackgroundCol, x - 7, this.table.Y, 2, this.table.Height);
      return columnWidth + 5;
    }

    private PackedCol GetGridCol(bool featured, bool selected)
    {
      if (!featured)
        return TableView.foreGridCol;
      if (selected)
        return new PackedCol(50, 53, 0);
      return new PackedCol(101, 107, 0);
    }

    private PackedCol GetGridColpl(bool dead, bool superquiet, bool quiet, bool typical, bool busy, bool popular, bool selected)
    {
      if (dead)
      {
        if (selected)
          return new PackedCol(64, 48, 36);
        return new PackedCol(64, 29, 2);
      }
      if (superquiet)
      {
        if (selected)
          return new PackedCol(222, 105, 73);
        return new PackedCol(222, 71, 29);
      }
      if (quiet)
      {
        if (selected)
          return new PackedCol(222, 172, 71);
        return new PackedCol(254, 182, 39);
      }
      if (typical)
      {
        if (selected)
          return new PackedCol(130, (int) byte.MaxValue, 132);
        return new PackedCol(0, (int) byte.MaxValue, 4);
      }
      if (busy)
      {
        if (selected)
          return new PackedCol(84, (int) byte.MaxValue, 201);
        return new PackedCol(0, (int) byte.MaxValue, 174);
      }
      if (!popular)
        return TableView.foreGridCol;
      if (selected)
        return new PackedCol(71, 218, (int) byte.MaxValue);
      return new PackedCol(0, 204, (int) byte.MaxValue);
    }

    private bool DrawColumnEntry(IDrawer2D drawer, ref DrawTextArgs args, int maxWidth, int x, ref int y, ref TableEntry entry)
    {
      Size size = drawer.MeasureText(ref args);
      bool flag = args.Text == "";
      if (flag)
        size.Height = this.entryHeight;
      if (y + size.Height > this.table.Y + this.table.Height)
      {
        y = this.table.Y + this.table.Height + 2;
        return false;
      }
      entry.Y = y;
      entry.Height = size.Height;
      if (!flag)
      {
        size.Width = Math.Min(maxWidth, size.Width);
        args.SkipPartsCheck = false;
        Drawer2DExt.DrawClippedText(ref args, drawer, x, y, maxWidth);
      }
      y += size.Height + 2;
      return true;
    }

    private void ResetEntries()
    {
      for (int index = 0; index < this.table.Count; ++index)
      {
        this.table.entries[index].Height = 0;
        this.table.entries[index].Y = -10;
      }
    }

    private void DrawGrid(IDrawer2D drawer)
    {
      if (!this.game.ClassicBackground)
        drawer.Clear(LauncherSkin.BackgroundCol, this.table.X, this.table.Y + this.headerHeight + 5, this.table.Width, 2);
      this.headerStartY = this.table.Y;
      this.headerEndY = this.table.Y + this.headerHeight + 5;
      this.numEntries = (this.table.Y + this.table.Height - (this.headerEndY + 3)) / (this.entryHeight + 3);
    }

    private void DrawScrollbar(IDrawer2D drawer)
    {
      PackedCol col1 = this.game.ClassicBackground ? new PackedCol(80, 80, 80) : LauncherSkin.ButtonBorderCol;
      drawer.Clear(col1, this.game.Width - 10, this.table.Y, 10, this.table.Height);
      PackedCol col2 = this.game.ClassicBackground ? new PackedCol(160, 160, 160) : LauncherSkin.ButtonForeActiveCol;
      int y;
      int height;
      this.table.GetScrollbarCoords(out y, out height);
      drawer.Clear(col2, this.game.Width - 10, this.table.Y + y, 10, height);
    }

    private delegate string ColumnFilter(TableEntry entry);
  }

}