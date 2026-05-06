import 'package:flutter/material.dart';
import 'package:intl/intl.dart';

import '../models/system_health.dart';
import '../services/api_service.dart';

class HealthScreen extends StatefulWidget {
  const HealthScreen({super.key});

  @override
  State<HealthScreen> createState() => _HealthScreenState();
}

class _HealthScreenState extends State<HealthScreen> {
  final _service = ApiService();
  SystemHealth? _health;

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
        title: const Text('System Health'),
        backgroundColor: const Color(0xFF0D0D0D),
        foregroundColor: Colors.greenAccent,
      ),
      body: StreamBuilder<Map<String, dynamic>>(
        stream: _service.healthStream,
        builder: (context, snapshot) {
          if (snapshot.hasData) {
            _health = SystemHealth.fromJson(snapshot.data!);
          }

          if (_health == null) {
            return const Center(
              child: Column(
                mainAxisAlignment: MainAxisAlignment.center,
                children: [
                  CircularProgressIndicator(color: Colors.greenAccent),
                  SizedBox(height: 16),
                  Text('Connecting to system...',
                      style: TextStyle(color: Colors.white54)),
                ],
              ),
            );
          }

          final h = _health!;
          return SingleChildScrollView(
            padding: const EdgeInsets.fromLTRB(16, 24, 16, 24),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.stretch,
              children: [
                _OverallBadge(status: h.overall),
                const SizedBox(height: 24),
                Text(
                  _overallSummary(h),
                  textAlign: TextAlign.center,
                  style: const TextStyle(
                      color: Colors.white70, fontSize: 16),
                ),
                const SizedBox(height: 32),
                ...h.subsystems.map((s) => _SubsystemCard(sub: s)),
                const SizedBox(height: 16),
                Text(
                  'Updated ${DateFormat('HH:mm:ss').format(h.timestamp)}',
                  textAlign: TextAlign.center,
                  style: const TextStyle(
                      color: Colors.white38, fontSize: 12),
                ),
              ],
            ),
          );
        },
      ),
    );
  }

  String _overallSummary(SystemHealth h) {
    final errors = h.subsystems.where((s) => s.status == 'error').length;
    final warns  = h.subsystems.where((s) => s.status == 'warn').length;
    if (errors > 0) {
      return '$errors subsystem${errors == 1 ? '' : 's'} need attention';
    }
    if (warns > 0) {
      return '$warns subsystem${warns == 1 ? '' : 's'} reporting warnings';
    }
    return 'All systems operational';
  }
}

// ─────────────────────────────────────────────────────────────────────────────

Color _statusColor(String status) {
  switch (status) {
    case 'error': return Colors.redAccent;
    case 'warn':  return Colors.orangeAccent;
    case 'ok':
    default:      return Colors.greenAccent;
  }
}

IconData _statusIcon(String status) {
  switch (status) {
    case 'error': return Icons.error_outline;
    case 'warn':  return Icons.warning_amber_rounded;
    case 'ok':
    default:      return Icons.check_circle_outline;
  }
}

String _statusLabel(String status) {
  switch (status) {
    case 'error': return 'CRITICAL';
    case 'warn':  return 'WARNING';
    case 'ok':
    default:      return 'HEALTHY';
  }
}

IconData _subsystemIcon(String id) {
  switch (id) {
    case 'cameras':    return Icons.videocam_outlined;
    case 'ultrasonic': return Icons.sensors_outlined;
    case 'yolo':       return Icons.psychology_outlined;
    case 'bins':       return Icons.delete_outline;
    case 'threads':    return Icons.memory_outlined;
    default:           return Icons.circle_outlined;
  }
}

// ─────────────────────────────────────────────────────────────────────────────

class _OverallBadge extends StatelessWidget {
  final String status;
  const _OverallBadge({required this.status});

  @override
  Widget build(BuildContext context) {
    final color = _statusColor(status);
    return Center(
      child: Container(
        width: 180,
        height: 180,
        decoration: BoxDecoration(
          shape: BoxShape.circle,
          gradient: RadialGradient(
            colors: [
              color.withAlpha(60),
              color.withAlpha(20),
              Colors.transparent,
            ],
            stops: const [0.0, 0.7, 1.0],
          ),
          border: Border.all(color: color, width: 3),
          boxShadow: [
            BoxShadow(
              color: color.withAlpha(80),
              blurRadius: 24,
              spreadRadius: 2,
            ),
          ],
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: [
            Icon(_statusIcon(status), color: color, size: 56),
            const SizedBox(height: 8),
            Text(
              _statusLabel(status),
              style: TextStyle(
                color: color,
                fontSize: 18,
                fontWeight: FontWeight.bold,
                letterSpacing: 2,
              ),
            ),
          ],
        ),
      ),
    );
  }
}

class _SubsystemCard extends StatelessWidget {
  final SubsystemHealth sub;
  const _SubsystemCard({required this.sub});

  @override
  Widget build(BuildContext context) {
    final color = _statusColor(sub.status);
    return Container(
      margin: const EdgeInsets.only(bottom: 8),
      padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 12),
      decoration: BoxDecoration(
        color: const Color(0xFF1A1A1A),
        borderRadius: BorderRadius.circular(10),
        border: Border.all(color: color.withAlpha(80)),
      ),
      child: Row(
        children: [
          Icon(_subsystemIcon(sub.id), color: color, size: 22),
          const SizedBox(width: 14),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(sub.label,
                    style: const TextStyle(
                      color: Colors.white,
                      fontSize: 14,
                      fontWeight: FontWeight.w600,
                    )),
                if (sub.detail.isNotEmpty)
                  Text(sub.detail,
                      style: const TextStyle(
                          color: Colors.white54, fontSize: 12)),
              ],
            ),
          ),
          Icon(_statusIcon(sub.status), color: color, size: 22),
        ],
      ),
    );
  }
}
