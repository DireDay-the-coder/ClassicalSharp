// Decompiled with JetBrains decompiler
// Type: Launcher.Gui.Widgets.TableWidget
// Assembly: Launcher2, Version=0.99.9.99, Culture=neutral, PublicKeyToken=null
// MVID: 9A5D6DDE-06CE-47A5-BCD3-CE89077CCD3C
// Assembly location: E:\Dires Dev Space\MCDire\working client - Copy\Launcher2.exe

using ClassicalSharp;
using Launcher.Web;
using System;
using System.Collections.Generic;
using System.Drawing;

namespace Launcher.Gui.Widgets
{
  public class TableWidget : Widget
  {
    public int SelectedIndex = -1;
    public string SelectedHash = "";
    public int[] ColumnWidths = new int[7]
    {
      30,
      320,
      65,
      65,
      95,
      65,
      365
    };
    public int[] DesiredColumnWidths = new int[7]
    {
      30,
      320,
      65,
      65,
      95,
      65,
      365
    };
    private DefaultComparer defComp = new DefaultComparer();
    private FlagComparer flagComp = new FlagComparer();
    private NameComparer nameComp = new NameComparer();
    private OnlineComparer onlineComp = new OnlineComparer();
    private MaxComparer maxComp = new MaxComparer();
    private UptimeComparer uptimeComp = new UptimeComparer();
    private IPComparer ipComp = new IPComparer();
    private SoftwareComparer softwareComp = new SoftwareComparer();
    internal int DraggingColumn = -1;
    internal bool DraggingScrollbar = false;
    private int lastIndex = -10;
    private TableView view;
    public TableNeedsRedrawHandler NeedRedraw;
    public Action<string> SelectedChanged;
    public int CurrentIndex;
    public int Count;
    internal TableEntry[] entries;
    internal int[] order;
    internal List<ServerListEntry> servers;
    private string curFilter;
    internal int mouseOffset;
    private DateTime lastPress;

    public TableWidget(LauncherWindow window)
      : base(window)
    {
      this.OnClick = new WidgetClickHandler(this.HandleOnClick);
      this.view = new TableView();
      this.view.Init(window, this);
    }

    internal TableEntry Get(int i)
    {
      return this.entries[this.order[i]];
    }

    public void SetEntries(List<ServerListEntry> servers)
    {
      this.entries = new TableEntry[servers.Count];
      this.order = new int[servers.Count];
      this.servers = servers;
      for (int index = 0; index < servers.Count; ++index)
      {
        ServerListEntry server = servers[index];
        this.entries[index] = new TableEntry()
        {
          Hash = server.Hash,
          Name = server.Name,
          Players = server.Players + "/" + server.MaxPlayers,
          Online = server.Players,
          Max = server.MaxPlayers,
          IP = server.IPAddress,
          Software = server.Software,
          Uptime = this.MakeUptime(server.Uptime),
          RawUptime = server.Uptime,
          Featured = server.Featured,
          Flag = server.Flag
        };
        this.order[index] = index;
      }
      this.Count = this.entries.Length;
    }

    public void FilterEntries(string filter)
    {
      this.curFilter = filter;
      this.Count = 0;
      for (int index = 0; index < this.entries.Length; ++index)
      {
        if (this.entries[index].Name.IndexOf(filter, StringComparison.OrdinalIgnoreCase) >= 0)
          this.order[this.Count++] = index;
      }
      for (int count = this.Count; count < this.entries.Length; ++count)
        this.order[count] = -100000;
    }

    internal void GetScrollbarCoords(out int y, out int height)
    {
      if (this.Count == 0)
      {
        y = 0;
        height = 0;
      }
      else
      {
        float num = (float) this.Height / (float) this.Count;
        y = (int) Math.Ceiling((double) this.CurrentIndex * (double) num);
        height = (int) Math.Ceiling((double) (this.view.maxIndex - this.CurrentIndex) * (double) num);
        height = Math.Min(y + height, this.Height) - y;
      }
    }

    public void SetSelected(int index)
    {
      if (index >= this.view.maxIndex)
        this.CurrentIndex = index + 1 - this.view.numEntries;
      if (index < this.CurrentIndex)
        this.CurrentIndex = index;
      if (index >= this.Count)
        index = this.Count - 1;
      if (index < 0)
        index = 0;
      this.SelectedHash = "";
      this.SelectedIndex = index;
      this.lastIndex = index;
      this.ClampIndex();
      if (this.Count <= 0)
        return;
      TableEntry tableEntry = this.Get(index);
      this.SelectedChanged(tableEntry.Hash);
      this.SelectedHash = tableEntry.Hash;
    }

    public void SetSelected(string hash)
    {
      this.SelectedIndex = -1;
      for (int index = 0; index < this.Count; ++index)
      {
        if (!(this.Get(index).Hash != hash))
        {
          this.SetSelected(index);
          break;
        }
      }
    }

    public void ClampIndex()
    {
      if (this.CurrentIndex > this.Count - this.view.numEntries)
        this.CurrentIndex = this.Count - this.view.numEntries;
      if (this.CurrentIndex >= 0)
        return;
      this.CurrentIndex = 0;
    }

    private string MakeUptime(string rawSeconds)
    {
      TimeSpan timeSpan = TimeSpan.FromSeconds(double.Parse(rawSeconds));
      if (timeSpan.TotalHours >= 168.0)
        return ((int) timeSpan.TotalDays).ToString() + "d";
      if (timeSpan.TotalHours >= 1.0)
        return ((int) timeSpan.TotalHours).ToString() + "h";
      if (timeSpan.TotalMinutes >= 1.0)
        return ((int) timeSpan.TotalMinutes).ToString() + "m";
      return ((int) timeSpan.TotalSeconds).ToString() + "s";
    }

    public void SetDrawData(IDrawer2D drawer, Font font, Font titleFont, Anchor horAnchor, Anchor verAnchor, int x, int y)
    {
      this.SetLocation(horAnchor, verAnchor, x, y);
      this.view.SetDrawData(drawer, font, titleFont);
    }

    public void RecalculateDrawData()
    {
      this.view.RecalculateDrawData();
    }

    public void RedrawData(IDrawer2D drawer)
    {
      this.view.RedrawData(drawer);
    }

    public void RedrawFlags()
    {
      this.view.DrawFlags();
    }

    public override void Redraw(IDrawer2D drawer)
    {
      this.RecalculateDrawData();
      this.RedrawData(drawer);
    }

    public void SortDefault()
    {
      this.SortEntries((TableEntryComparer) this.defComp, true);
    }

    private void SelectHeader(int mouseX, int mouseY)
    {
      int num = this.X + 15;
      for (int index = 0; index < this.ColumnWidths.Length; ++index)
      {
        num += this.ColumnWidths[index] + 10;
        if (mouseX >= num - 8 && mouseX < num + 8)
        {
          this.DraggingColumn = index;
          this.lastIndex = -10;
          return;
        }
      }
      this.TrySortColumns(mouseX);
    }

    private void TrySortColumns(int mouseX)
    {
      int num1 = this.X + 15;
      if (mouseX >= num1 && mouseX < num1 + this.ColumnWidths[0])
      {
        this.SortEntries((TableEntryComparer) this.flagComp, false);
      }
      else
      {
        int num2 = num1 + (this.ColumnWidths[0] + 10);
        if (mouseX >= num2 && mouseX < num2 + this.ColumnWidths[1])
        {
          this.SortEntries((TableEntryComparer) this.nameComp, false);
        }
        else
        {
          int num3 = num2 + (this.ColumnWidths[1] + 10);
          if (mouseX >= num3 && mouseX < num3 + this.ColumnWidths[2])
          {
            this.SortEntries((TableEntryComparer) this.onlineComp, false);
          }
          else
          {
            int num4 = num3 + (this.ColumnWidths[2] + 10);
            if (mouseX >= num4 && mouseX < num4 + this.ColumnWidths[3])
            {
              this.SortEntries((TableEntryComparer) this.maxComp, false);
            }
            else
            {
              int num5 = num4 + (this.ColumnWidths[3] + 10);
              if (mouseX >= num5 && mouseX < num5 + this.ColumnWidths[4])
              {
                this.SortEntries((TableEntryComparer) this.uptimeComp, false);
              }
              else
              {
                int num6 = num5 + (this.ColumnWidths[4] + 10);
                if (mouseX >= num6 && mouseX < num6 + this.ColumnWidths[5])
                {
                  this.SortEntries((TableEntryComparer) this.ipComp, false);
                }
                else
                {
                  int num7 = num6 + (this.ColumnWidths[5] + 10);
                  if (mouseX < num7)
                    return;
                  this.SortEntries((TableEntryComparer) this.softwareComp, false);
                }
              }
            }
          }
        }
      }
    }

    private void SortEntries(TableEntryComparer comparer, bool noRedraw)
    {
      Array.Sort<TableEntry>(this.entries, 0, this.entries.Length, (IComparer<TableEntry>) comparer);
      this.lastIndex = -10;
      if (this.curFilter != null && this.curFilter.Length > 0)
        this.FilterEntries(this.curFilter);
      if (noRedraw)
        return;
      comparer.Invert = !comparer.Invert;
      this.SetSelected(this.SelectedHash);
      this.NeedRedraw();
    }

    private void GetSelectedServer(int mouseX, int mouseY)
    {
      for (int index = 0; index < this.Count; ++index)
      {
        TableEntry tableEntry = this.Get(index);
        if (mouseY >= tableEntry.Y && mouseY < tableEntry.Y + tableEntry.Height + 2)
        {
          if (this.lastIndex == index && (DateTime.UtcNow - this.lastPress).TotalSeconds < 1.0)
          {
            this.Window.ConnectToServer(this.servers, tableEntry.Hash);
            this.lastPress = DateTime.MinValue;
            break;
          }
          this.SetSelected(index);
          this.NeedRedraw();
          break;
        }
      }
    }

    private void HandleOnClick(int mouseX, int mouseY)
    {
      if (mouseX >= this.Window.Width - 10)
      {
        this.ScrollbarClick(mouseY);
        this.DraggingScrollbar = true;
        this.lastIndex = -10;
      }
      else
      {
        if (mouseY >= this.view.headerStartY && mouseY < this.view.headerEndY)
          this.SelectHeader(mouseX, mouseY);
        else
          this.GetSelectedServer(mouseX, mouseY);
        this.lastPress = DateTime.UtcNow;
      }
    }

    public void MouseMove(int x, int y, int deltaX, int deltaY)
    {
      if (this.DraggingScrollbar)
      {
        y -= this.Y;
        float num = (float) this.Height / (float) this.Count;
        this.CurrentIndex = (int) ((double) (y - this.mouseOffset) / (double) num);
        this.ClampIndex();
        this.NeedRedraw();
      }
      else
      {
        if (this.DraggingColumn < 0 || x >= this.Window.Width - 20)
          return;
        int draggingColumn = this.DraggingColumn;
        this.ColumnWidths[draggingColumn] += deltaX;
        Utils.Clamp(ref this.ColumnWidths[draggingColumn], 20, this.Window.Width - 20);
        this.DesiredColumnWidths[draggingColumn] = this.ColumnWidths[draggingColumn];
        this.NeedRedraw();
      }
    }

    private void ScrollbarClick(int mouseY)
    {
      mouseY -= this.Y;
      int y;
      int height;
      this.GetScrollbarCoords(out y, out height);
      int num = this.view.maxIndex - this.CurrentIndex;
      if (mouseY < y)
        this.CurrentIndex -= num;
      else if (mouseY >= y + height)
      {
        this.CurrentIndex += num;
      }
      else
      {
        this.DraggingScrollbar = true;
        this.mouseOffset = mouseY - y;
      }
      this.ClampIndex();
      this.NeedRedraw();
    }
  }
}
