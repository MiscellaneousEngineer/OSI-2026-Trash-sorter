import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import '../services/websocket_service.dart';
import '../models/detection.dart';

class DetectionsScreen extends StatefulWidget {
  const DetectionsScreen({super.key});
  @override
  State<DetectionsScreen> createState() => _DetectionsScreenState();
}

class _DetectionsScreenState extends State<DetectionsScreen> {
  final _service = WebSocketService();
  final List<Detection> _log = [];

  @override
  void dispose() {
    _service.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: const Color(0xFF0D0D0D),
      appBar: AppBar(
        title: Text('Detection Log (${_log.length})'),
        backgroundColor: const Color(0xFF0D0D0D),
        foregroundColor: Colors.greenAccent,
        actions: [
          IconButton(
            icon: const Icon(Icons.delete_outline),
            onPressed: () => setState(() => _log.clear()),
          ),
        ],
      ),
      body: StreamBuilder<Map<String, dynamic>>(
        stream: _service.detectionStream,
        builder: (context, snapshot) {
          if (snapshot.hasData) {
            final timestamp = DateTime.fromMillisecondsSinceEpoch(
                ((snapshot.data!['timestamp'] ?? 0) * 1000).toInt());
            final objects =
                snapshot.data!['objects'] as List<dynamic>? ?? [];
            for (var obj in objects) {
              setState(() {
                _log.insert(0, Detection.fromJson(obj, timestamp));
              });
            }
            if (_log.length > 200) _log.removeRange(200, _log.length);
          }

          if (_log.isEmpty) {
            return const Center(
              child: Text('Waiting for detections...',
                  style: TextStyle(color: Colors.white54)),
            );
          }

          return ListView.builder(
            padding: const EdgeInsets.all(12),
            itemCount: _log.length,
            itemBuilder: (ctx, i) {
              final d = _log[i];
              return Container(
                margin: const EdgeInsets.only(bottom: 8),
                padding: const EdgeInsets.all(12),
                decoration: BoxDecoration(
                  color: const Color(0xFF1A1A1A),
                  borderRadius: BorderRadius.circular(10),
                  border: Border.all(
                      color: Colors.greenAccent.withOpacity(0.2)),
                ),
                child: Row(children: [
                  const Icon(Icons.visibility,
                      color: Colors.greenAccent, size: 20),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(d.label.toUpperCase(),
                            style: const TextStyle(
                                color: Colors.white,
                                fontWeight: FontWeight.bold)),
                        Text(DateFormat('HH:mm:ss').format(d.timestamp),
                            style: const TextStyle(
                                color: Colors.white38, fontSize: 11)),
                      ],
                    ),
                  ),
                  Text(
                    '${(d.confidence * 100).toStringAsFixed(1)}%',
                    style: TextStyle(
                      color: d.confidence > 0.8
                          ? Colors.greenAccent
                          : Colors.orangeAccent,
                      fontWeight: FontWeight.bold,
                    ),
                  ),
                ]),
              );
            },
          );
        },
      ),
    );
  }
}