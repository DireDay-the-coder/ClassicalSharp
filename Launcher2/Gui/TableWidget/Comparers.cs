// ClassicalSharp copyright 2014-2016 UnknownShadow200 | Licensed under MIT
using System;
using System.Collections.Generic;

namespace Launcher.Gui.Widgets {
	
	abstract class TableEntryComparer : IComparer<TableEntry> {
		
		public bool Invert = true;
		
		public abstract int Compare(TableEntry a, TableEntry b);
	}
	
	sealed class DefaultComparer : TableEntryComparer {
		
		public override int Compare(TableEntry a, TableEntry b) {
			long valX = Int64.Parse(a.Online);
			long valY = Int64.Parse(b.Online);
			int value = valY.CompareTo(valX);
			if (value != 0) return value;
			
			long timeX = Int64.Parse(a.RawUptime);
			long timeY = Int64.Parse(b.RawUptime);
			return timeY.CompareTo(timeX);
		}
	}

    sealed class FlagComparer : TableEntryComparer {

        public override int Compare(TableEntry a, TableEntry b) {
            StringComparison comparison = StringComparison.CurrentCultureIgnoreCase;
            int value = String.Compare(a.Flag, b.Flag, comparison);
            return Invert ? -value : value;
        }
    }

    sealed class NameComparer : TableEntryComparer {
		
		public override int Compare(TableEntry a, TableEntry b) {
			StringComparison comparison = StringComparison.CurrentCultureIgnoreCase;
			int value = String.Compare(a.Name, b.Name, comparison);
			return Invert ? -value : value;
		}
	}
	
	sealed class PlayersComparer : TableEntryComparer {
		
		public override int Compare(TableEntry a, TableEntry b) {
			long valX = Int64.Parse(a.Online);
			long valY = Int64.Parse(b.Online);
			int value = valX.CompareTo(valY);
			return Invert ? -value : value;
		}
	}

    sealed class OnlineComparer : TableEntryComparer {

        public override int Compare(TableEntry a, TableEntry b) {
            StringComparison comparison = StringComparison.CurrentCultureIgnoreCase;
            int value = String.Compare(a.Online, b.Online, comparison);
            return Invert ? -value : value;
        }
    }

    sealed class MaxComparer : TableEntryComparer {

        public override int Compare(TableEntry a, TableEntry b) {
            StringComparison comparison = StringComparison.CurrentCultureIgnoreCase;
            int value = String.Compare(a.Max, b.Max, comparison);
            return Invert ? -value : value;
        }
    }

    sealed class UptimeComparer : TableEntryComparer {
		
		public override int Compare(TableEntry a, TableEntry b) {
			long timeX = Int64.Parse(a.RawUptime);
			long timeY = Int64.Parse(b.RawUptime);
			int value = timeX.CompareTo(timeY);
			return Invert ? -value : value;
		}
	}

    sealed class IPComparer : TableEntryComparer {

        public override int Compare(TableEntry a, TableEntry b) {
            StringComparison comparison = StringComparison.InvariantCulture;
            int value = String.Compare(a.IP, b.IP, comparison);
            return Invert ? -value : value;
        }
    }

    sealed class SoftwareComparer : TableEntryComparer {
		
		public override int Compare(TableEntry a, TableEntry b) {
			StringComparison comparison = StringComparison.CurrentCultureIgnoreCase;
			int value = String.Compare(a.Software, b.Software, comparison);
			return Invert ? -value : value;
		}
	}
}
