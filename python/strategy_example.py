#!/usr/bin/env python3
"""
Ultra-Fast Analysis Order Matching Engine - Example Strategy

This example demonstrates how to create a simple algorithmic trading strategy
using the C++ order matching engine via Python bindings.
"""

import time
import random
import numpy as np
from typing import Dict, List, Optional
from dataclasses import dataclass
from datetime import datetime

# Import the C++ engine bindings
try:
    from order_engine_python import (
        OrderMatchingEngine, Order, MarketData, OrderBookSnapshot,
        PerformanceMonitor, ORDER_SIDE_BUY, ORDER_SIDE_SELL,
        ORDER_TYPE_LIMIT, MARKET_DATA_TYPE_TICK
    )
except ImportError:
    print("Error: Could not import order_engine_python module.")
    print("Make sure the C++ engine is built with Python bindings enabled.")
    exit(1)


@dataclass
class Position:
    """Represents a trading position for a symbol."""
    symbol: str
    quantity: int
    avg_price: float
    pnl: float = 0.0
    
    def update_pnl(self, current_price: float):
        """Update PnL based on current market price."""
        if self.quantity > 0:  # Long position
            self.pnl = (current_price - self.avg_price) * self.quantity
        else:  # Short position
            self.pnl = (self.avg_price - current_price) * abs(self.quantity)


class SimpleArbitrageStrategy:
    """
    A simple arbitrage strategy that looks for price differences between
    bid and ask prices and places orders when the spread is favorable.
    """
    
    def __init__(self, engine: OrderMatchingEngine, symbols: List[str]):
        self.engine = engine
        self.symbols = symbols
        self.positions: Dict[str, Position] = {}
        self.order_id_counter = 10000
        self.min_spread_threshold = 0.02  # 2 cents minimum spread
        self.max_position_size = 1000
        
        # Initialize positions
        for symbol in symbols:
            self.positions[symbol] = Position(symbol, 0, 0.0)
        
        # Set up market data callback
        self.engine.set_market_data_callback(self.on_market_data)
        
        print(f"Strategy initialized for symbols: {symbols}")
    
    def on_market_data(self, market_data: MarketData):
        """Handle incoming market data updates."""
        symbol = market_data.symbol
        if symbol not in self.symbols:
            return
        
        # Update position PnL
        if symbol in self.positions:
            self.positions[symbol].update_pnl(market_data.price)
        
        # Check for arbitrage opportunities
        self.check_arbitrage_opportunities(symbol)
    
    def check_arbitrage_opportunities(self, symbol: str):
        """Check if there are arbitrage opportunities for a symbol."""
        try:
            snapshot = self.engine.get_order_book_snapshot(symbol)
            
            if not snapshot.bids or not snapshot.asks:
                return
            
            # Get best bid and ask
            best_bid = snapshot.bids[0]["price"]
            best_ask = snapshot.asks[0]["price"]
            
            spread = best_ask - best_bid
            
            if spread > self.min_spread_threshold:
                print(f"Arbitrage opportunity detected for {symbol}:")
                print(f"  Best bid: {best_bid:.2f}")
                print(f"  Best ask: {best_ask:.2f}")
                print(f"  Spread: {spread:.2f}")
                
                # Place arbitrage orders
                self.place_arbitrage_orders(symbol, best_bid, best_ask)
        
        except Exception as e:
            print(f"Error checking arbitrage for {symbol}: {e}")
    
    def place_arbitrage_orders(self, symbol: str, bid_price: float, ask_price: float):
        """Place buy and sell orders for arbitrage."""
        position = self.positions[symbol]
        
        # Calculate order quantities based on current position
        if position.quantity >= self.max_position_size:
            # Already at max position, can only sell
            self.place_order(symbol, ORDER_SIDE_SELL, ORDER_TYPE_LIMIT, 100, ask_price)
        elif position.quantity <= -self.max_position_size:
            # Already at max short position, can only buy
            self.place_order(symbol, ORDER_SIDE_BUY, ORDER_TYPE_LIMIT, 100, bid_price)
        else:
            # Can place both orders
            self.place_order(symbol, ORDER_SIDE_BUY, ORDER_TYPE_LIMIT, 100, bid_price)
            self.place_order(symbol, ORDER_SIDE_SELL, ORDER_TYPE_LIMIT, 100, ask_price)
    
    def place_order(self, symbol: str, side: str, order_type: str, 
                   quantity: int, price: float) -> bool:
        """Place an order with the engine."""
        try:
            order = Order(
                order_id=self.order_id_counter,
                client_id=1,  # Strategy client ID
                symbol=symbol,
                side=side,
                type=order_type,
                quantity=quantity,
                price=price
            )
            
            success = self.engine.submit_order(order)
            if success:
                self.order_id_counter += 1
                print(f"Order placed: {side} {quantity} {symbol} @ {price:.2f}")
                return True
            else:
                print(f"Failed to place order: {side} {quantity} {symbol} @ {price:.2f}")
                return False
        
        except Exception as e:
            print(f"Error placing order: {e}")
            return False
    
    def get_portfolio_summary(self) -> Dict:
        """Get a summary of the current portfolio."""
        total_pnl = 0.0
        total_value = 0.0
        
        for symbol, position in self.positions.items():
            total_pnl += position.pnl
            # Get current market price for position value
            try:
                snapshot = self.engine.get_order_book_snapshot(symbol)
                if snapshot.bids and snapshot.asks:
                    mid_price = (snapshot.bids[0]["price"] + snapshot.asks[0]["price"]) / 2
                    total_value += abs(position.quantity) * mid_price
            except:
                pass
        
        return {
            "total_pnl": total_pnl,
            "total_value": total_value,
            "positions": {s: {"quantity": p.quantity, "avg_price": p.avg_price, "pnl": p.pnl} 
                         for s, p in self.positions.items()}
        }
    
    def print_status(self):
        """Print current strategy status."""
        print("\n=== Strategy Status ===")
        print(f"Timestamp: {datetime.now()}")
        
        # Get engine metrics
        metrics = self.engine.get_performance_metrics()
        print(f"Orders Processed: {metrics['orders_processed']}")
        print(f"Trades Executed: {metrics['trades_executed']}")
        print(f"Avg Latency: {metrics['avg_latency_microseconds']:.2f} μs")
        
        # Get portfolio summary
        portfolio = self.get_portfolio_summary()
        print(f"Total PnL: ${portfolio['total_pnl']:.2f}")
        print(f"Portfolio Value: ${portfolio['total_value']:.2f}")
        
        print("\nPositions:")
        for symbol, pos_data in portfolio['positions'].items():
            print(f"  {symbol}: {pos_data['quantity']} @ ${pos_data['avg_price']:.2f} "
                  f"(PnL: ${pos_data['pnl']:.2f})")
        
        print("=====================")


class MarketMakingStrategy:
    """
    A simple market making strategy that maintains bid and ask orders
    around the current market price.
    """
    
    def __init__(self, engine: OrderMatchingEngine, symbols: List[str]):
        self.engine = engine
        self.symbols = symbols
        self.spread_multiplier = 0.001  # 0.1% spread
        self.order_size = 100
        self.active_orders: Dict[str, List[int]] = {symbol: [] for symbol in symbols}
        
        # Set up market data callback
        self.engine.set_market_data_callback(self.on_market_data)
        
        print(f"Market Making Strategy initialized for symbols: {symbols}")
    
    def on_market_data(self, market_data: MarketData):
        """Handle incoming market data updates."""
        symbol = market_data.symbol
        if symbol not in self.symbols:
            return
        
        # Update market making orders
        self.update_market_making_orders(symbol)
    
    def update_market_making_orders(self, symbol: str):
        """Update market making orders for a symbol."""
        try:
            snapshot = self.engine.get_order_book_snapshot(symbol)
            
            if not snapshot.bids or not snapshot.asks:
                return
            
            # Calculate mid price
            mid_price = (snapshot.bids[0]["price"] + snapshot.asks[0]["price"]) / 2
            
            # Calculate bid and ask prices
            bid_price = mid_price * (1 - self.spread_multiplier)
            ask_price = mid_price * (1 + self.spread_multiplier)
            
            # Cancel existing orders
            for order_id in self.active_orders[symbol]:
                self.engine.cancel_order(order_id, symbol)
            self.active_orders[symbol].clear()
            
            # Place new orders
            bid_order = Order(20000 + len(self.active_orders[symbol]), 2, symbol, 
                            ORDER_SIDE_BUY, ORDER_TYPE_LIMIT, self.order_size, bid_price)
            ask_order = Order(30000 + len(self.active_orders[symbol]), 2, symbol, 
                            ORDER_SIDE_SELL, ORDER_TYPE_LIMIT, self.order_size, ask_price)
            
            if self.engine.submit_order(bid_order):
                self.active_orders[symbol].append(bid_order.order_id)
            if self.engine.submit_order(ask_order):
                self.active_orders[symbol].append(ask_order.order_id)
            
            print(f"Updated market making orders for {symbol}: "
                  f"Bid @ {bid_price:.2f}, Ask @ {ask_price:.2f}")
        
        except Exception as e:
            print(f"Error updating market making orders for {symbol}: {e}")


def run_strategy_demo():
    """Run a demonstration of the trading strategies."""
    print("=== Ultra-Fast Analysis Strategy Demo ===")
    
    # Create the order matching engine
    engine = OrderMatchingEngine(num_matching_threads=4, num_market_data_threads=2)
    
    try:
        # Start the engine
        if not engine.start():
            print("Failed to start order matching engine")
            return
        
        print("Order matching engine started successfully")
        
        # Define symbols to trade
        symbols = ["AAPL", "GOOGL", "MSFT"]
        
        # Create strategies
        arbitrage_strategy = SimpleArbitrageStrategy(engine, symbols)
        market_making_strategy = MarketMakingStrategy(engine, symbols)
        
        print("Strategies created and running...")
        print("Press Ctrl+C to stop...")
        
        # Main loop
        start_time = time.time()
        while True:
            try:
                # Print status every 10 seconds
                if int(time.time() - start_time) % 10 == 0:
                    arbitrage_strategy.print_status()
                
                time.sleep(1)
                
            except KeyboardInterrupt:
                print("\nShutting down...")
                break
        
        # Stop the engine
        engine.stop()
        print("Strategy demo completed")
        
    except Exception as e:
        print(f"Error in strategy demo: {e}")
        engine.stop()


if __name__ == "__main__":
    run_strategy_demo()
