"""
trade_reader.py - reads a trade csv one line at a time (Python version of trade_io.c)

Every line has 7 fields:
    trade id, price, qty, quoteQty, time (microseconds), isBuyerMaker, isBestMatch
"""


class TradeReader:

    def __init__(self, path):
        self.path = path
        self.file = None
        self.skipped = 0       # lines that were not trades (header row, blank line ...)

        # the current trade, filled in by next_trade()
        self.number = 0        # 1, 2, 3 ... counts real trades only (this is "t" in the brief)
        self.trade_id = 0
        self.price = 0.0
        self.qty = 0.0
        self.quote_qty = 0.0   # price * qty from Binance, not used but trade_io.c reads it too
        self.time_us = 0       # microseconds since 1 Jan 1970
        self.is_buy = 0        # 1 if a buyer started the trade (isBuyerMaker is False)

    def open(self):
        # returns True if the file opened, False if not
        try:
            self.file = open(self.path, "r")
        except OSError:
            print("TradeReader: cannot open", self.path)
            print("TradeReader: check the csv paths in Python/settings.py")
            return False
        print("TradeReader: reading", self.path)
        return True

    def next_trade(self):
        """Read the next trade into self.price, self.qty ... Returns False at the end of the file."""
        while True:
            line = self.file.readline()
            if line == "":
                return False               # end of file

            fields = line.strip().split(",")
            if len(fields) < 7:
                self.skipped += 1          # blank or broken line
                continue

            # NOTE: this is a bit more forgiving than the sscanf in trade_io.c on
            # broken lines (e.g. an empty isBestMatch). The Binance files never have those.
            try:
                trade_id = int(fields[0])
                price = float(fields[1])
                qty = float(fields[2])
                quote_qty = float(fields[3])
                time_us = int(fields[4])
            except ValueError:
                self.skipped += 1          # e.g. a header row "id,price,qty,..."
                continue

            self.trade_id = trade_id
            self.price = price
            self.qty = qty
            self.quote_qty = quote_qty
            self.time_us = time_us

            # isBuyerMaker False means the buyer started it, so it counts as a BUY.
            # Same check as trade_io.c, only the first letter (F or f) matters.
            if fields[5].lower().startswith("f"):
                self.is_buy = 1
            else:
                self.is_buy = 0

            self.number += 1
            return True

    def close(self):
        if self.file is not None:
            self.file.close()
            self.file = None
        if self.skipped > 0:
            print(f"TradeReader: {self.skipped} line(s) skipped because they were not trades.")
