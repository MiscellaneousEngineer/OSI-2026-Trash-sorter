import 'package:flutter/material.dart';
import 'package:fl_chart/fl_chart.dart';
import '../services/websocket_service.dart';
import '../models/sensor_data.dart';

class SensorsScreen extends StatefulWidget {
  const SensorsScreen({super.key});
  @override
  State<SensorsScreen> createState() => _SensorsScreenState();
}

class _SensorsScreenState extends State<SensorsScreen> {
  final _service = WebSocketService();
  final List<FlSpot> _tempHistory = [];
  int _tick = 0;

  @override
  void dispose() {
    _service.dispose();
    super.dispose();
  }

  Widget statCard(String label, String value, Color color) {
    return Expanded(
      child: Container(
        padding: const EdgeInsets.all(12),
        decoration: BoxDecoration(
          color: const Color(0xFF1A1A1A),
          borderRadius: BorderRadius.circular(12),
          border: Border.all(color: color.withAlpha(100)),
        ),
        child: Column(
          children: [
            Text(label,
                style: const TextStyle(
                    color: Colors.white54, fontSize: 11)),
            const SizedBox(height: 6),
            Text(value,
                style: TextStyle(
                    color: color,
                    fontSize: 18,
                    fontWeight: FontWeight.bold)),
          ],
        ),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0D0D0D),
      appBar: AppBar(
        title: const Text('Sensors'),
        backgroundColor: const Color(0xFF0D0D0D),
        foregroundColor: Colors.greenAccent,
      ),
      body: StreamBuilder<Map<String, dynamic>>(
        stream: _service.sensorStream,
        builder: (context, snapshot) {
          if (!snapshot.hasData) {
            return const Center(
              child: CircularProgressIndicator(
                color: Colors.greenAccent,
              ),
            );
          }

          final data = SensorData.fromJson(snapshot.data!);
          _tempHistory.add(
              FlSpot(_tick++.toDouble(), data.temperature));
          if (_tempHistory.length > 30) _tempHistory.removeAt(0);

          return Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    statCard(
                      'CPU Temp',
                      '${data.temperature.toStringAsFixed(1)}°C',
                      Colors.orangeAccent,
                    ),
                    const SizedBox(width: 12),
                    statCard(
                      'GPU Usage',
                      '${data.gpuUsage.toStringAsFixed(1)}%',
                      Colors.blueAccent,
                    ),
                    const SizedBox(width: 12),
                    statCard(
                      'CPU Usage',
                      '${data.cpuUsage.toStringAsFixed(1)}%',
                      Colors.purpleAccent,
                    ),
                  ],
                ),
                const SizedBox(height: 24),
                const Text(
                  'CPU Temperature History',
                  style: TextStyle(color: Colors.white70),
                ),
                const SizedBox(height: 8),
                SizedBox(
                  height: 200,
                  child: LineChart(
                    LineChartData(
                      gridData: const FlGridData(show: false),
                      borderData: FlBorderData(show: false),
                      titlesData: const FlTitlesData(show: false),
                      lineBarsData: [
                        LineChartBarData(
                          spots: _tempHistory.isEmpty
                              ? [const FlSpot(0, 0)]
                              : _tempHistory,
                          isCurved: true,
                          color: Colors.orangeAccent,
                          dotData: const FlDotData(show: false),
                          belowBarData: BarAreaData(
                            show: true,
                            color: Colors.orangeAccent.withAlpha(25),
                          ),
                        ),
                      ],
                    ),
                  ),
                ),
                const SizedBox(height: 24),
                const Text(
                  'IMU Data',
                  style: TextStyle(color: Colors.white70),
                ),
                const SizedBox(height: 8),
                Row(
                  children: [
                    statCard(
                      'Accel X',
                      data.imu['ax']?.toStringAsFixed(2) ?? '0.00',
                      Colors.greenAccent,
                    ),
                    const SizedBox(width: 12),
                    statCard(
                      'Accel Y',
                      data.imu['ay']?.toStringAsFixed(2) ?? '0.00',
                      Colors.greenAccent,
                    ),
                    const SizedBox(width: 12),
                    statCard(
                      'Accel Z',
                      data.imu['az']?.toStringAsFixed(2) ?? '0.00',
                      Colors.greenAccent,
                    ),
                  ],
                ),
              ],
            ),
          );
        },
      ),
    );
  }
}