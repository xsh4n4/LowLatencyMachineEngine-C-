#!/usr/bin/env python3
"""
Ultra-Fast Analysis Order Matching Engine - Visualization Tools

This module provides visualization capabilities for analyzing:
- Order book depth and structure
- Performance metrics and latency distributions
- Strategy performance and PnL
- Market data patterns
"""

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from typing import Dict, List, Optional, Tuple
import seaborn as sns
from datetime import datetime, timedelta
import json

# Set style for better-looking plots
plt.style.use('seaborn-v0_8')
sns.set_palette("husl")


class OrderBookVisualizer:
    """Visualize order book depth and structure."""
    
    def __init__(self):
        self.fig, self.axes = plt.subplots(2, 2, figsize=(15, 10))
        self.fig.suptitle('Order Book Visualization', fontsize=16)
    
    def plot_order_book_depth(self, snapshot, symbol: str):
        """Plot order book depth chart."""
        ax = self.axes[0, 0]
        ax.clear()
        
        if not snapshot.bids or not snapshot.asks:
            ax.text(0.5, 0.5, 'No order book data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title(f'{symbol} - Order Book Depth')
            return
        
        # Extract bid and ask data
        bid_prices = [bid["price"] for bid in snapshot.bids]
        bid_quantities = [bid["quantity"] for bid in snapshot.bids]
        ask_prices = [ask["price"] for ask in snapshot.asks]
        ask_quantities = [ask["quantity"] for ask in snapshot.asks]
        
        # Create cumulative quantities
        bid_cumulative = np.cumsum(bid_quantities)
        ask_cumulative = np.cumsum(ask_quantities)
        
        # Plot
        ax.plot(bid_prices, bid_cumulative, 'g-', linewidth=2, label='Bids', alpha=0.7)
        ax.plot(ask_prices, ask_cumulative, 'r-', linewidth=2, label='Asks', alpha=0.7)
        
        # Fill areas
        ax.fill_between(bid_prices, bid_cumulative, alpha=0.3, color='green')
        ax.fill_between(ask_prices, ask_cumulative, alpha=0.3, color='red')
        
        ax.set_xlabel('Price')
        ax.set_ylabel('Cumulative Quantity')
        ax.set_title(f'{symbol} - Order Book Depth')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        # Add spread information
        spread = ask_prices[0] - bid_prices[0]
        mid_price = (bid_prices[0] + ask_prices[0]) / 2
        ax.text(0.02, 0.98, f'Spread: ${spread:.2f}\nMid: ${mid_price:.2f}', 
               transform=ax.transAxes, verticalalignment='top',
               bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.8))
    
    def plot_price_levels(self, snapshot, symbol: str):
        """Plot individual price levels."""
        ax = self.axes[0, 1]
        ax.clear()
        
        if not snapshot.bids or not snapshot.asks:
            ax.text(0.5, 0.5, 'No order book data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title(f'{symbol} - Price Levels')
            return
        
        # Extract data
        bid_prices = [bid["price"] for bid in snapshot.bids]
        bid_quantities = [bid["quantity"] for bid in snapshot.bids]
        ask_prices = [ask["price"] for ask in snapshot.asks]
        ask_quantities = [ask["quantity"] for ask in snapshot.asks]
        
        # Create horizontal bar chart
        y_pos_bids = np.arange(len(bid_prices))
        y_pos_asks = np.arange(len(ask_prices))
        
        ax.barh(y_pos_bids, bid_quantities, color='green', alpha=0.7, label='Bids')
        ax.barh(-y_pos_asks-1, ask_quantities, color='red', alpha=0.7, label='Asks')
        
        # Set labels
        ax.set_yticks(np.concatenate([-y_pos_asks-1, y_pos_bids]))
        ax.set_yticklabels([f'${p:.2f}' for p in ask_prices[::-1] + bid_prices])
        
        ax.set_xlabel('Quantity')
        ax.set_title(f'{symbol} - Price Levels')
        ax.legend()
        ax.grid(True, alpha=0.3)
    
    def plot_spread_analysis(self, snapshot, symbol: str):
        """Plot spread analysis."""
        ax = self.axes[1, 0]
        ax.clear()
        
        if not snapshot.bids or not snapshot.asks:
            ax.text(0.5, 0.5, 'No order book data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title(f'{symbol} - Spread Analysis')
            return
        
        # Calculate spreads at different levels
        max_levels = min(len(snapshot.bids), len(snapshot.asks), 10)
        levels = range(1, max_levels + 1)
        spreads = []
        
        for i in range(max_levels):
            spread = snapshot.asks[i]["price"] - snapshot.bids[i]["price"]
            spreads.append(spread)
        
        # Plot spread by level
        ax.plot(levels, spreads, 'bo-', linewidth=2, markersize=6)
        ax.fill_between(levels, spreads, alpha=0.3, color='blue')
        
        ax.set_xlabel('Price Level')
        ax.set_ylabel('Spread ($)')
        ax.set_title(f'{symbol} - Spread by Price Level')
        ax.grid(True, alpha=0.3)
        
        # Add statistics
        avg_spread = np.mean(spreads)
        ax.axhline(y=avg_spread, color='red', linestyle='--', alpha=0.7, 
                  label=f'Avg: ${avg_spread:.2f}')
        ax.legend()
    
    def plot_quantity_distribution(self, snapshot, symbol: str):
        """Plot quantity distribution."""
        ax = self.axes[1, 1]
        ax.clear()
        
        if not snapshot.bids or not snapshot.asks:
            ax.text(0.5, 0.5, 'No order book data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title(f'{symbol} - Quantity Distribution')
            return
        
        # Extract quantities
        bid_quantities = [bid["quantity"] for bid in snapshot.bids]
        ask_quantities = [ask["quantity"] for ask in snapshot.asks]
        
        # Create histogram
        ax.hist(bid_quantities, bins=20, alpha=0.7, color='green', label='Bids', density=True)
        ax.hist(ask_quantities, bins=20, alpha=0.7, color='red', label='Asks', density=True)
        
        ax.set_xlabel('Quantity')
        ax.set_ylabel('Density')
        ax.set_title(f'{symbol} - Quantity Distribution')
        ax.legend()
        ax.grid(True, alpha=0.3)
    
    def update(self, snapshot, symbol: str):
        """Update all plots with new data."""
        self.plot_order_book_depth(snapshot, symbol)
        self.plot_price_levels(snapshot, symbol)
        self.plot_spread_analysis(snapshot, symbol)
        self.plot_quantity_distribution(snapshot, symbol)
        plt.tight_layout()
    
    def show(self):
        """Display the plots."""
        plt.show()


class PerformanceVisualizer:
    """Visualize performance metrics and latency distributions."""
    
    def __init__(self):
        self.fig, self.axes = plt.subplots(2, 3, figsize=(18, 12))
        self.fig.suptitle('Performance Metrics Visualization', fontsize=16)
        
        # Store historical data
        self.latency_history = []
        self.throughput_history = []
        self.timestamps = []
    
    def plot_latency_trend(self, latencies: List[float], timestamps: List[datetime]):
        """Plot latency trend over time."""
        ax = self.axes[0, 0]
        ax.clear()
        
        if not latencies:
            ax.text(0.5, 0.5, 'No latency data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('Latency Trend')
            return
        
        ax.plot(timestamps, latencies, 'b-', linewidth=2, alpha=0.7)
        ax.fill_between(timestamps, latencies, alpha=0.3, color='blue')
        
        ax.set_xlabel('Time')
        ax.set_ylabel('Latency (μs)')
        ax.set_title('Latency Trend Over Time')
        ax.grid(True, alpha=0.3)
        
        # Rotate x-axis labels for better readability
        plt.setp(ax.get_xticklabels(), rotation=45)
    
    def plot_throughput_metrics(self, metrics: Dict):
        """Plot throughput metrics."""
        ax = self.axes[0, 1]
        ax.clear()
        
        # Extract throughput data
        categories = ['Orders/sec', 'Trades/sec', 'Market Data/sec']
        values = [
            metrics.get('orders_per_second', 0),
            metrics.get('trades_per_second', 0),
            metrics.get('market_data_per_second', 0)
        ]
        
        bars = ax.bar(categories, values, color=['blue', 'green', 'orange'], alpha=0.7)
        
        # Add value labels on bars
        for bar, value in zip(bars, values):
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height + height*0.01,
                   f'{value:,}', ha='center', va='bottom')
        
        ax.set_ylabel('Throughput (per second)')
        ax.set_title('Throughput Metrics')
        ax.grid(True, alpha=0.3)
    
    def plot_latency_distribution(self, latencies: List[float]):
        """Plot latency distribution histogram."""
        ax = self.axes[0, 2]
        ax.clear()
        
        if not latencies:
            ax.text(0.5, 0.5, 'No latency data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('Latency Distribution')
            return
        
        ax.hist(latencies, bins=30, alpha=0.7, color='skyblue', edgecolor='black')
        ax.axvline(np.mean(latencies), color='red', linestyle='--', 
                  label=f'Mean: {np.mean(latencies):.2f} μs')
        ax.axvline(np.median(latencies), color='orange', linestyle='--', 
                  label=f'Median: {np.median(latencies):.2f} μs')
        
        ax.set_xlabel('Latency (μs)')
        ax.set_ylabel('Frequency')
        ax.set_title('Latency Distribution')
        ax.legend()
        ax.grid(True, alpha=0.3)
    
    def plot_performance_summary(self, metrics: Dict):
        """Plot performance summary."""
        ax = self.axes[1, 0]
        ax.clear()
        
        # Create a summary table
        summary_data = [
            ['Orders Processed', f"{metrics.get('orders_processed', 0):,}"],
            ['Trades Executed', f"{metrics.get('trades_executed', 0):,}"],
            ['Market Data Updates', f"{metrics.get('market_data_updates', 0):,}"],
            ['Avg Latency', f"{metrics.get('avg_latency_microseconds', 0):.2f} μs"]
        ]
        
        table = ax.table(cellText=summary_data, colLabels=['Metric', 'Value'],
                        cellLoc='center', loc='center')
        table.auto_set_font_size(False)
        table.set_fontsize(12)
        table.scale(1, 2)
        
        ax.set_title('Performance Summary')
        ax.axis('off')
    
    def plot_memory_usage(self, memory_data: List[Tuple[datetime, float]]):
        """Plot memory usage over time."""
        ax = self.axes[1, 1]
        ax.clear()
        
        if not memory_data:
            ax.text(0.5, 0.5, 'No memory data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('Memory Usage')
            return
        
        timestamps, memory_values = zip(*memory_data)
        ax.plot(timestamps, memory_values, 'g-', linewidth=2, alpha=0.7)
        ax.fill_between(timestamps, memory_values, alpha=0.3, color='green')
        
        ax.set_xlabel('Time')
        ax.set_ylabel('Memory Usage (MB)')
        ax.set_title('Memory Usage Over Time')
        ax.grid(True, alpha=0.3)
        
        plt.setp(ax.get_xticklabels(), rotation=45)
    
    def plot_cpu_usage(self, cpu_data: List[Tuple[datetime, float]]):
        """Plot CPU usage over time."""
        ax = self.axes[1, 2]
        ax.clear()
        
        if not cpu_data:
            ax.text(0.5, 0.5, 'No CPU data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('CPU Usage')
            return
        
        timestamps, cpu_values = zip(*cpu_data)
        ax.plot(timestamps, cpu_values, 'r-', linewidth=2, alpha=0.7)
        ax.fill_between(timestamps, cpu_values, alpha=0.3, color='red')
        
        ax.set_xlabel('Time')
        ax.set_ylabel('CPU Usage (%)')
        ax.set_title('CPU Usage Over Time')
        ax.grid(True, alpha=0.3)
        
        plt.setp(ax.get_xticklabels(), rotation=45)
    
    def update(self, metrics: Dict, latency: float = None, 
               memory_mb: float = None, cpu_percent: float = None):
        """Update all plots with new data."""
        current_time = datetime.now()
        
        if latency is not None:
            self.latency_history.append(latency)
            self.timestamps.append(current_time)
            
            # Keep only last 1000 data points
            if len(self.latency_history) > 1000:
                self.latency_history.pop(0)
                self.timestamps.pop(0)
        
        self.plot_throughput_metrics(metrics)
        self.plot_latency_distribution(self.latency_history)
        self.plot_performance_summary(metrics)
        
        if self.timestamps:
            self.plot_latency_trend(self.latency_history, self.timestamps)
        
        # Update memory and CPU plots if data available
        if memory_mb is not None:
            if not hasattr(self, 'memory_data'):
                self.memory_data = []
            self.memory_data.append((current_time, memory_mb))
            if len(self.memory_data) > 100:
                self.memory_data.pop(0)
            self.plot_memory_usage(self.memory_data)
        
        if cpu_percent is not None:
            if not hasattr(self, 'cpu_data'):
                self.cpu_data = []
            self.cpu_data.append((current_time, cpu_percent))
            if len(self.cpu_data) > 100:
                self.cpu_data.pop(0)
            self.plot_cpu_usage(self.cpu_data)
        
        plt.tight_layout()
    
    def show(self):
        """Display the plots."""
        plt.show()


class StrategyPerformanceVisualizer:
    """Visualize strategy performance and PnL."""
    
    def __init__(self):
        self.fig, self.axes = plt.subplots(2, 2, figsize=(15, 10))
        self.fig.suptitle('Strategy Performance Visualization', fontsize=16)
        
        # Store historical data
        self.pnl_history = []
        self.position_history = []
        self.timestamps = []
    
    def plot_pnl_trend(self, pnl_data: List[Tuple[datetime, float]]):
        """Plot PnL trend over time."""
        ax = self.axes[0, 0]
        ax.clear()
        
        if not pnl_data:
            ax.text(0.5, 0.5, 'No PnL data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('PnL Trend')
            return
        
        timestamps, pnl_values = zip(*pnl_data)
        ax.plot(timestamps, pnl_values, 'g-', linewidth=2, alpha=0.7)
        ax.fill_between(timestamps, pnl_values, alpha=0.3, color='green')
        
        # Add zero line
        ax.axhline(y=0, color='black', linestyle='-', alpha=0.5)
        
        ax.set_xlabel('Time')
        ax.set_ylabel('PnL ($)')
        ax.set_title('Profit & Loss Trend')
        ax.grid(True, alpha=0.3)
        
        plt.setp(ax.get_xticklabels(), rotation=45)
    
    def plot_position_sizes(self, position_data: List[Tuple[datetime, Dict]]):
        """Plot position sizes over time."""
        ax = self.axes[0, 1]
        ax.clear()
        
        if not position_data:
            ax.text(0.5, 0.5, 'No position data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('Position Sizes')
            return
        
        timestamps, positions = zip(*position_data)
        
        # Extract symbols and their quantities
        symbols = set()
        for pos_dict in positions:
            symbols.update(pos_dict.keys())
        
        for symbol in symbols:
            quantities = [pos_dict.get(symbol, {}).get('quantity', 0) for pos_dict in positions]
            ax.plot(timestamps, quantities, linewidth=2, alpha=0.7, label=symbol)
        
        ax.set_xlabel('Time')
        ax.set_ylabel('Position Size')
        ax.set_title('Position Sizes Over Time')
        ax.legend()
        ax.grid(True, alpha=0.3)
        
        plt.setp(ax.get_xticklabels(), rotation=45)
    
    def plot_returns_distribution(self, returns: List[float]):
        """Plot returns distribution."""
        ax = self.axes[1, 0]
        ax.clear()
        
        if not returns:
            ax.text(0.5, 0.5, 'No returns data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('Returns Distribution')
            return
        
        ax.hist(returns, bins=30, alpha=0.7, color='lightcoral', edgecolor='black')
        ax.axvline(np.mean(returns), color='red', linestyle='--', 
                  label=f'Mean: {np.mean(returns):.4f}')
        ax.axvline(np.median(returns), color='orange', linestyle='--', 
                  label=f'Median: {np.median(returns):.4f}')
        
        ax.set_xlabel('Returns')
        ax.set_ylabel('Frequency')
        ax.set_title('Returns Distribution')
        ax.legend()
        ax.grid(True, alpha=0.3)
    
    def plot_risk_metrics(self, pnl_data: List[Tuple[datetime, float]]):
        """Plot risk metrics."""
        ax = self.axes[1, 1]
        ax.clear()
        
        if not pnl_data:
            ax.text(0.5, 0.5, 'No PnL data', ha='center', va='center', 
                   transform=ax.transAxes)
            ax.set_title('Risk Metrics')
            return
        
        # Calculate risk metrics
        _, pnl_values = zip(*pnl_data)
        pnl_values = np.array(pnl_values)
        
        # Calculate daily returns (assuming data is daily)
        if len(pnl_values) > 1:
            returns = np.diff(pnl_values) / pnl_values[:-1]
            
            # Risk metrics
            volatility = np.std(returns) * np.sqrt(252)  # Annualized
            sharpe_ratio = np.mean(returns) / np.std(returns) * np.sqrt(252) if np.std(returns) > 0 else 0
            max_drawdown = np.min(pnl_values - np.maximum.accumulate(pnl_values))
            
            # Create summary table
            summary_data = [
                ['Volatility', f'{volatility:.2%}'],
                ['Sharpe Ratio', f'{sharpe_ratio:.2f}'],
                ['Max Drawdown', f'${max_drawdown:.2f}'],
                ['Total Return', f'${pnl_values[-1] - pnl_values[0]:.2f}']
            ]
            
            table = ax.table(cellText=summary_data, colLabels=['Metric', 'Value'],
                            cellLoc='center', loc='center')
            table.auto_set_font_size(False)
            table.set_fontsize(12)
            table.scale(1, 2)
            
            ax.set_title('Risk Metrics Summary')
            ax.axis('off')
    
    def update(self, portfolio_summary: Dict):
        """Update all plots with new data."""
        current_time = datetime.now()
        
        # Update PnL data
        total_pnl = portfolio_summary.get('total_pnl', 0.0)
        self.pnl_history.append((current_time, total_pnl))
        if len(self.pnl_history) > 1000:
            self.pnl_history.pop(0)
        
        # Update position data
        positions = portfolio_summary.get('positions', {})
        self.position_history.append((current_time, positions))
        if len(self.position_history) > 1000:
            self.position_history.pop(0)
        
        # Update plots
        self.plot_pnl_trend(self.pnl_history)
        self.plot_position_sizes(self.position_history)
        
        # Calculate returns for distribution
        if len(self.pnl_history) > 1:
            returns = []
            for i in range(1, len(self.pnl_history)):
                prev_pnl = self.pnl_history[i-1][1]
                curr_pnl = self.pnl_history[i][1]
                if prev_pnl != 0:
                    returns.append((curr_pnl - prev_pnl) / abs(prev_pnl))
            
            if returns:
                self.plot_returns_distribution(returns)
        
        self.plot_risk_metrics(self.pnl_history)
        
        plt.tight_layout()
    
    def show(self):
        """Display the plots."""
        plt.show()


def create_dashboard(order_book_snapshots: Dict[str, any], 
                    performance_metrics: Dict, 
                    portfolio_summary: Dict):
    """Create a comprehensive dashboard with all visualizations."""
    # Create order book visualizer
    ob_viz = OrderBookVisualizer()
    
    # Create performance visualizer
    perf_viz = PerformanceVisualizer()
    
    # Create strategy performance visualizer
    strat_viz = StrategyPerformanceVisualizer()
    
    # Update visualizations with data
    for symbol, snapshot in order_book_snapshots.items():
        ob_viz.update(snapshot, symbol)
        break  # Just show first symbol for now
    
    perf_viz.update(performance_metrics)
    strat_viz.update(portfolio_summary)
    
    # Show all visualizations
    plt.show()
    
    return ob_viz, perf_viz, strat_viz


if __name__ == "__main__":
    # Example usage
    print("Order Matching Engine Visualization Tools")
    print("Import this module to use the visualization classes in your strategies.")
