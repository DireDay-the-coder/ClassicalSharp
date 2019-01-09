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
        internal static PackedCol backGridCol = new PackedCol(20, 20, 10),
        foreGridCol = new PackedCol(40, 40, 40);
    
        int entryHeight, headerHeight;
    
        internal int headerStartY, headerEndY;
        internal int numEntries, maxIndex;

        LauncherWindow game;
        TableWidget table;
        Font font, titleFont;

        public void Init(LauncherWindow game, TableWidget table) {
            this.game = game;
            this.table = table;
        }

        public void SetDrawData(IDrawer2D drawer, Font font, Font titleFont)
        {
            this.font = font;
            this.titleFont = titleFont;
            headerHeight = drawer.FontHeight(titleFont, true);
            entryHeight = drawer.FontHeight(font, true);
        }

        public void RecalculateDrawData() {
            for (int i = 0; i < table.ColumnWidths.Length; i++)
            {
                table.ColumnWidths[i] = this.table.DesiredColumnWidths[i];
                Utils.Clamp(ref this.table.ColumnWidths[i], 20, this.game.Width - 20);
            }
            table.Width = this.game.Width - this.table.X;
            ResetEntries();

            int y = table.Y + 3;
            y += headerHeight + 2;
            maxIndex = table.Count;
            y += 5;

            for (int i = table.CurrentIndex; i < table.Count; i++) {
                    if (y + entryHeight > table.Y + table.Height) {
                            maxIndex = i; return;

                    }

                    table.entries[this.table.order[i]].Y = y;
                    table.entries[this.table.order[i]].Height = this.entryHeight;
                    y += this.entryHeight + 2;
            }
        }   

        public const int flagPadding = 15;
        public void RedrawData(IDrawer2D drawer) {
               DrawGrid(drawer);

               int x = table.X + flagPadding + 5;
               x += DrawColumn (drawer, "Flag", 0, x, TableView.filterFlag) + 5;
               x += DrawColumn (drawer, "Name", 1, x, TableView.filterName) + 5;
               x += DrawColumn (drawer, "Online", 2, x, TableView.filterOnline) + 5;
               x += DrawColumn (drawer, "Max Slots", 3, x, TableView.filterMax) + 5;
               x += DrawColumn (drawer, "Uptime", 4, x, TableView.filterUptime) + 5;
               x += DrawColumn (drawer, "IP Address", 5, x, TableView.filterIP) + 5;
               x += DrawColumn (drawer, "Software", 6, x, TableView.filterSoftware) + 5;

           DrawScrollbar(drawer);
           DrawFlags();
    }

    public void Redraw(IDrawer2D drawer) {
      RecalculateDrawData();
      RedrawData(drawer);
    }

    delegate string ColumnFilter(TableEntry entry);
      // cache to avoid allocations every redraw
    static string FilterFlag(TableEntry e) { return e.Flag; } static ColumnFilter filterFlag = FilterFlag;
    static string FilterName(TableEntry e) { return e.Name; } static ColumnFilter filterName = FilterName;
    static string FilterOnline(TableEntry e) { return e.Online; } static ColumnFilter filterOnline = FilterOnline;
    static string FilterMax(TableEntry e) { return e.Max; } static ColumnFilter filterMax = FilterMax;
    static string FilterUptime(TableEntry e) { return e.Uptime; } static ColumnFilter filterUptime = FilterUptime;
    static string FilterIP(TableEntry e) { return e.IP; } static ColumnFilter filterIP = FilterIP;
    static string FilterSoftware(TableEntry e) { return e.Software; } static ColumnFilter filterSoftware = FilterSoftware;

    static FastBitmap GetFlag(string flag) {
           List<string> flags = FetchFlagsTask.Flags;
           List<FastBitmap> bitmaps = FetchFlagsTask.Bitmaps;

           for (int i = 0; i < flags.Count; i++) {
                    if (flags[i] != flag) continue;
                    return i < bitmaps.Count ? bitmaps[i] : null;
           }
           return null;
    }

    public void DrawFlags() {
           using (FastBitmap dst = game.LockBits()) {
                    for (int i = table.CurrentIndex; i < maxIndex; i++) {
                            TableEntry entry = table.Get(i);
                            FastBitmap flag = GetFlag(entry.Flag);
                            if (flag == null) continue;

                            int x = table.X, y = entry.Y;
                            Rectangle rect = new Rectangle(x + 2, y + 3, 16, 11);
                            BitmapDrawer.Draw(flag, dst, rect);
                    }
            }
    }

    

    private int DrawColumn(IDrawer2D drawer, string header, int columnI, int x, TableView.ColumnFilter filter)
    {
      int y = table.Y + 3;
      int maxWidth = table.ColumnWidths[columnI];
      bool seperator = columnI > 0;

      DrawTextArgs args = new DrawTextArgs(header, titleFont, true);
      TableEntry headerEntry = default(TableEntry);
      DrawColumnEntry(drawer, ref args, maxWidth, x, ref y, ref headerEntry);
      maxIndex = table.Count;

      y += 5;
      for (int i = table.CurrentIndex; i < table.Count; i++) {
        TableEntry entry = table.Get(i);
        args = new DrawTextArgs(filter(entry), font, true);
        int players = int.Parse(entry.Online);
        if ((i == table.SelectedIndex || entry.Featured || players == 0 || (players >= 1 && players < 3 || players >= 3 && players < 5) || (players >= 5 && players < 7 || players >= 7 && players < 10 || players >= 10 && players < 20 || players >= 20)) && !seperator) {
          int startY = y - 3;
          int height = Math.Min(y + (entryHeight + 4), table.Y + table.Height) - y;
          drawer.Clear(this.GetGridCol(entry.Featured, i == table.SelectedIndex), table.X, startY, 65, height);
          drawer.Clear(this.GetGridColpl(players == 0, players >= 1 && players < 3, players >= 3 && players < 5, players >= 5 && players < 7, players >= 7 && players < 10, players >= 10 && players < 20, players >= 20, i == table.SelectedIndex), table.X, startY, table.Width, height);
        }
        if (!this.DrawColumnEntry(drawer, ref args, maxWidth, x, ref y, ref entry)) {
          maxIndex = i; break;
        }
      }
    
      if (seperator && !this.game.ClassicBackground)
        drawer.Clear(LauncherSkin.BackgroundCol, x - 7, this.table.Y, 2, this.table.Height);
      return maxWidth + 5;
    }

    private PackedCol GetGridCol(bool featured, bool selected) {
           if (!featured)
           return TableView.foreGridCol;

           if (selected)
           return new PackedCol(50, 53, 0);
           return new PackedCol(101, 107, 0);
    }

    private PackedCol GetGridColpl(bool dead, bool superquiet, bool quiet, bool typical, bool busy, bool popular, bool superpopular, bool selected) {
            if (dead) {
                if (selected)
                return new PackedCol(64, 48, 36);
            return new PackedCol(64, 29, 2);
            }

            if (superquiet) {
                if (selected)
                return new PackedCol(222, 105, 73);
            return new PackedCol(222, 71, 29);
            }

            if (quiet) {
               if (selected)
               return new PackedCol(222, 172, 71);
            return new PackedCol(254, 182, 39);
            }

            if (typical) {
                if (selected)
                return new PackedCol(130, (int) byte.MaxValue, 132);
             return new PackedCol(0, (int) byte.MaxValue, 4);
            }

            if (busy) {
                if (selected)
                return new PackedCol(84, (int) byte.MaxValue, 201);
            return new PackedCol(0, (int) byte.MaxValue, 174);
            }

            if (popular) {
                if (selected)
                return new PackedCol(71, 218, (int) byte.MaxValue);
            return new PackedCol(0, 204, (int) byte.MaxValue);
            }

            if (superpopular)
            {
                if (selected)
                return new PackedCol(168, 238, (int)byte.MaxValue);
            return new PackedCol(168, 238, (int)byte.MaxValue);
            }

            return foreGridCol;
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
      headerStartY = this.table.Y;
      headerEndY = this.table.Y + this.headerHeight + 5;
      numEntries = (this.table.Y + this.table.Height - (this.headerEndY + 3)) / (this.entryHeight + 3);
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
  }
}