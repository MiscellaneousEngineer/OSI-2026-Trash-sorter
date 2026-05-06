import 'package:flutter/material.dart';
import 'package:intl/intl.dart';
import '../models/bin_status.dart';
import '../services/websocket_service.dart';

class BinsScreen extends StatefulWidget {
  const BinsScreen({super.key});

  @override
  State<BinsScreen> createState() => _BinsScreenState();
}

class _BinsScreenState extends State<BinsScreen> {
  final _service = WebSocketService();
  List<BinStatus> _bins = [];
  DateTime? _lastUpdate;

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
        title: const Text('Bin Levels'),
        backgroundColor: const Color(0xFF0D0D0D),
        foregroundColor: Colors.greenAccent,
      ),
      body: StreamBuilder<Map<String, dynamic>>(
        stream: _service.binStream,
        builder: (context, snapshot) {
          if (snapshot.hasData) {
            final data = snapshot.data!;
            _bins = (data['bins'] as List? ?? [])
                .map((b) => BinStatus.fromJson(b as Map<String, dynamic>))
                .toList();
            _lastUpdate = DateTime.fromMillisecondsSinceEpoch(
                ((data['timestamp'] ?? 0) * 1000).toInt());
          }

          if (_bins.isEmpty) {
            return const Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  CircularProgressIndicator(color: Colors.greenAccent),
                  SizedBox(height: 16),
                  Text('Waiting for bin data...',
                      style: TextStyle(color: Colors.white54)),
                ],
              ),
            );
          }

          return Padding(
            padding: const EdgeInsets.fromLTRB(12, 16, 12, 24),
            child: Column(
              children: [
                if (_lastUpdate != null)
                  Text(
                    'Updated ${DateFormat('HH:mm:ss').format(_lastUpdate!)}',
                    style: const TextStyle(
                        color: Colors.white38, fontSize: 12),
                  ),
                const SizedBox(height: 20),
                Expanded(
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.stretch,
                    children: _bins
                        .map((b) => Expanded(child: _BinWidget(bin: b)))
                        .toList(),
                  ),
                ),
              ],
            ),
          );
        },
      ),
    );
  }
}

class _BinWidget extends StatelessWidget {
  final BinStatus bin;
  const _BinWidget({required this.bin});

  Color get _fillColor {
    if (bin.fillPercent > 85) return Colors.redAccent;
    if (bin.fillPercent > 60) return Colors.orangeAccent;
    return Colors.greenAccent;
  }

  IconData get _icon {
    switch (bin.id.toLowerCase()) {
      case 'metal':   return Icons.hardware;
      case 'glass':   return Icons.wine_bar_outlined;
      case 'plastic': return Icons.local_drink_outlined;
      case 'paper':   return Icons.description_outlined;
      default:        return Icons.delete_outline;
    }
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(horizontal: 4),
      child: Column(
        children: [
          Icon(_icon, color: _fillColor, size: 22),
          const SizedBox(height: 6),
          Expanded(
            child: LayoutBuilder(
              builder: (context, constraints) {
                final fraction =
                    (bin.fillPercent / 100).clamp(0.0, 1.0);
                final fillHeight = constraints.maxHeight * fraction;
                return Container(
                  width: double.infinity,
                  decoration: BoxDecoration(
                    color: const Color(0xFF1A1A1A),
                    border: Border.all(color: Colors.white24, width: 2),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: ClipRRect(
                    borderRadius: BorderRadius.circular(6),
                    child: Stack(
                      children: [
                        Align(
                          alignment: Alignment.bottomCenter,
                          child: AnimatedContainer(
                            duration: const Duration(milliseconds: 400),
                            curve: Curves.easeOut,
                            height: fillHeight,
                            decoration: BoxDecoration(
                              gradient: LinearGradient(
                                begin: Alignment.topCenter,
                                end: Alignment.bottomCenter,
                                colors: [
                                  _fillColor.withAlpha(150),
                                  _fillColor.withAlpha(220),
                                ],
                              ),
                            ),
                          ),
                        ),
                        Center(
                          child: Text(
                            '${bin.fillPercent.toStringAsFixed(0)}%',
                            style: const TextStyle(
                              color: Colors.white,
                              fontWeight: FontWeight.bold,
                              fontSize: 16,
                              shadows: [
                                Shadow(
                                    blurRadius: 4,
                                    color: Colors.black87,
                                    offset: Offset(0, 1)),
                              ],
                            ),
                          ),
                        ),
                      ],
                    ),
                  ),
                );
              },
            ),
          ),
          const SizedBox(height: 8),
          Text(
            bin.label,
            style: const TextStyle(color: Colors.white, fontSize: 11),
            textAlign: TextAlign.center,
            overflow: TextOverflow.ellipsis,
          ),
          if (bin.capacityL > 0)
            Text(
              '${bin.filledL.toStringAsFixed(1)}/${bin.capacityL.toStringAsFixed(0)} L',
              style: const TextStyle(color: Colors.white38, fontSize: 10),
              textAlign: TextAlign.center,
            ),
        ],
      ),
    );
  }
}
