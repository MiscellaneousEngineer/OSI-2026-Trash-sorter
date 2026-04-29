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
                  color: Colors.greenAccent),