//+------------------------------------------------------------------+
//|                                              PyramidManager.mq4  |
//|                    Zarządzanie piramidą i Break-Even (GUI)       |
//+------------------------------------------------------------------+
#property strict

// --- Parametry zewnętrzne
input int MagicNumber = 0;       
input int ActivationPips = 2;    

// --- Zmienne globalne stanu
bool Global_CloseNetZero   = false;
bool Global_CloseEachZero  = false;
bool Global_MoneySL_Active = false; 
double Global_MoneySL_Value = 0; 
datetime lastModificationTime = 0; 

//+------------------------------------------------------------------+
//| Inicjalizacja                                                    |
//+------------------------------------------------------------------+
int OnInit() {
   ObjectDelete(0, "GUI_SPACER"); 
   CreateGUI();
   return(INIT_SUCCEEDED);
}

void OnDeinit(const int reason) {
   ObjectsDeleteAll(0, "BTN_"); 
   ObjectDelete(0, "EDT_MONEY_VAL");
}

//+------------------------------------------------------------------+
//| Główna pętla programu                                            |
//+------------------------------------------------------------------+
void OnTick() {
   double totalNetProfit = 0;
   int activeMarketPositions = 0;
   bool pendingToClear = false; 
   double pipsToPoints = (Digits == 3 || Digits == 5) ? Point * 10 : Point;

   // --- 1. SKANOWANIE POZYCJI ---
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            int type = OrderType();
            if(type == OP_BUY || type == OP_SELL) {
               double profit = OrderProfit() + OrderSwap() + OrderCommission();
               totalNetProfit += profit;
               activeMarketPositions++;

               if(Global_CloseEachZero) {
                  double openPrice = OrderOpenPrice();
                  double currentPrice = (type == OP_BUY) ? Bid : Ask;
                  double pipsProfit = (type == OP_BUY) ? (currentPrice - openPrice) : (openPrice - currentPrice);
                  pipsProfit /= pipsToPoints;

                  // Each Position Net Zero Logic
                  if(profit <= 0 && pipsProfit < ActivationPips && OrderProfit() != 0) { 
                     if(OrderClose(OrderTicket(), OrderLots(), OrderClosePrice(), 3, clrRed)) {
                        pendingToClear = true;
                     }
                  }
               }
            }
         }
      }
   }

   // --- 2. LOGIKA MONEY STOP LOSS ---
   if(Global_MoneySL_Active && activeMarketPositions > 0) {
      if(TimeCurrent() > lastModificationTime) {
          RefreshGlobalSL();
          lastModificationTime = TimeCurrent();
      }

      Global_MoneySL_Value = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));

      if(totalNetProfit <= Global_MoneySL_Value) {
          Print("Money Stop Loss osiągnięty! Zamykanie.");
          CloseAll();
          DeleteAllPending();
          ResetAllModes();
      }
   }

   if(pendingToClear) DeleteAllPending();

   // --- 3. PIRAMIDA NET ZERO ---
   if(Global_CloseNetZero && activeMarketPositions >= 2 && totalNetProfit <= 0) {
      CloseAll(); 
      DeleteAllPending();
      ResetAllModes();
   }
   
   if(activeMarketPositions == 0 && (Global_MoneySL_Active || Global_CloseNetZero || Global_CloseEachZero)) {
       DeleteAllPending();
   }
}

//+------------------------------------------------------------------+
//| Funkcje Zarządzania SL                                           |
//+------------------------------------------------------------------+
void RefreshGlobalSL() {
   if(!Global_MoneySL_Active) return;

   double targetLoss = StringToDouble(ObjectGetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT));
   if(targetLoss >= 0) return; 

   double totalLots = 0, weightedOpenPrice = 0;
   int type = -1, count = 0;

   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderType() <= 1) {
               totalLots += OrderLots();
               weightedOpenPrice += OrderOpenPrice() * OrderLots();
               type = OrderType();
               count++;
            }
         }
      }
   }

   if(count == 0 || totalLots == 0) return;
   double avgOpenPrice = weightedOpenPrice / totalLots;
   double tickVal = MarketInfo(_Symbol, MODE_TICKVALUE);
   double tickSz  = MarketInfo(_Symbol, MODE_TICKSIZE);
   
   if(tickVal <= 0 || tickSz <= 0) return;
   double pointValueInMoney = tickVal / tickSz;
   double priceDistance = MathAbs(targetLoss) / (totalLots * pointValueInMoney);
   
   double slPrice = (type == OP_BUY) ? avgOpenPrice - priceDistance : avgOpenPrice + priceDistance;
   slPrice = NormalizeDouble(slPrice, Digits);

   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(MathAbs(OrderStopLoss() - slPrice) > Point) {
               OrderModify(OrderTicket(), OrderOpenPrice(), slPrice, OrderTakeProfit(), 0, clrOrange);
            }
         }
      }
   }
}

void RemoveAllStopLosses() {
    for(int i = OrdersTotal() - 1; i >= 0; i--) {
        if(OrderSelect(i, SELECT_BY_POS) && OrderSymbol() == Symbol() && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderStopLoss() != 0) {
                RefreshRates();
                OrderModify(OrderTicket(), OrderOpenPrice(), 0, OrderTakeProfit(), 0, clrNONE);
            }
        }
    }
}

//+------------------------------------------------------------------+
//| Obsługa GUI                                                      |
//+------------------------------------------------------------------+
void OnChartEvent(const int id, const long &lparam, const double &dparam, const string &sparam) {
   if(id == CHARTEVENT_OBJECT_CLICK) {
      if(sparam == "BTN_NET_ZERO") {
         Global_CloseNetZero = !Global_CloseNetZero;
         UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", Global_CloseNetZero);
      }
      if(sparam == "BTN_EACH_ZERO") {
         Global_CloseEachZero = !Global_CloseEachZero;
         UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", Global_CloseEachZero);
      }
      if(sparam == "BTN_MONEY_SL") {
         Global_MoneySL_Active = !Global_MoneySL_Active;
         UpdateButton("BTN_MONEY_SL", "Money SL Mode", Global_MoneySL_Active);
      }
      
      if(sparam == "BTN_REMOVE_SL") {
         ResetAllModes();
         RemoveAllStopLosses();
         UpdateButton("BTN_REMOVE_SL", "SL REMOVED", true);
         ChartRedraw();
      }

      if(sparam == "BTN_PANIC") {
         CloseAll(); 
         DeleteAllPending();
         ResetAllModes();
         ObjectSetInteger(0, "BTN_PANIC", OBJPROP_STATE, false);
      }
      ChartRedraw();
   }
}

void ResetAllModes() {
    Global_CloseNetZero = false;
    Global_CloseEachZero = false;
    Global_MoneySL_Active = false;
    UpdateButton("BTN_NET_ZERO", "Pyramid Net Zero", false);
    UpdateButton("BTN_EACH_ZERO", "Each position Net Zero", false);
    UpdateButton("BTN_MONEY_SL", "Money SL Mode", false);
}

void CreateGUI() {
   CreateButton("BTN_NET_ZERO", 180, 30, "Pyramid Net Zero", Global_CloseNetZero);
   CreateButton("BTN_EACH_ZERO", 180, 70, "Each position Net Zero", Global_CloseEachZero);
   CreateButton("BTN_MONEY_SL", 180, 110, "Money SL Mode", Global_MoneySL_Active);
   
   if(ObjectFind(0, "EDT_MONEY_VAL") < 0) {
      ObjectCreate(0, "EDT_MONEY_VAL", OBJ_EDIT, 0, 0, 0);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_CORNER, CORNER_RIGHT_UPPER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XDISTANCE, 180);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YDISTANCE, 148); 
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_XSIZE, 170);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_YSIZE, 25);
      ObjectSetString(0, "EDT_MONEY_VAL", OBJPROP_TEXT, "-10.00");
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_ALIGN, ALIGN_CENTER);
      ObjectSetInteger(0, "EDT_MONEY_VAL", OBJPROP_BGCOLOR, clrWhite);
   }

   CreateButton("BTN_REMOVE_SL", 180, 185, "REMOVE ALL SL", false);
   ObjectSetInteger(0, "BTN_REMOVE_SL", OBJPROP_BGCOLOR, clrDarkOrange);
   CreateButton("BTN_PANIC", 180, 230, "CLOSE ALL", false); 
   ObjectSetInteger(0, "BTN_PANIC", OBJPROP_BGCOLOR, clrRed);
}

void CreateButton(string name, int x, int y, string text, bool state) {
   ObjectCreate(0, name, OBJ_BUTTON, 0, 0, 0);
   ObjectSetInteger(0, name, OBJPROP_CORNER, CORNER_RIGHT_UPPER);
   ObjectSetInteger(0, name, OBJPROP_XDISTANCE, x);
   ObjectSetInteger(0, name, OBJPROP_YDISTANCE, y);
   ObjectSetInteger(0, name, OBJPROP_XSIZE, 170);
   ObjectSetInteger(0, name, OBJPROP_YSIZE, 35);
   ObjectSetString(0, name, OBJPROP_TEXT, text);
   ObjectSetInteger(0, name, OBJPROP_FONTSIZE, 9);
   UpdateButton(name, text, state);
}

void UpdateButton(string name, string text, bool state) {
   color btnColor;
   
   if(state) {
      // Kolory dla stanów AKTYWNYCH
      if(name == "BTN_PANIC")      btnColor = clrRed;
      else if(name == "BTN_REMOVE_SL") btnColor = clrDarkOrange;
      else                         btnColor = clrDodgerBlue;
   } else {
      // Kolor dla stanu WYŁĄCZONEGO
      btnColor = clrGray;
   }

   ObjectSetInteger(0, name, OBJPROP_BGCOLOR, btnColor);
   ObjectSetInteger(0, name, OBJPROP_COLOR, clrWhite);
   ObjectSetInteger(0, name, OBJPROP_STATE, state); 
}

void CloseAll() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            int type = OrderType();
            if(type == OP_BUY)  OrderClose(OrderTicket(), OrderLots(), Bid, 3, clrOrange);
            if(type == OP_SELL) OrderClose(OrderTicket(), OrderLots(), Ask, 3, clrOrange);
         }
      }
   }
}

void DeleteAllPending() {
   for(int i = OrdersTotal() - 1; i >= 0; i--) {
      if(OrderSelect(i, SELECT_BY_POS, MODE_TRADES)) {
         if(OrderSymbol() == _Symbol && (MagicNumber == 0 || OrderMagicNumber() == MagicNumber)) {
            if(OrderType() > 1) OrderDelete(OrderTicket());
         }
      }
   }
}