/// Overall system health summary, coming from `GET /health` on the Jetson.
///
/// Expected payload:
/// ```json
/// {
///   "timestamp": 1714400000.123,
///   "overall": "ok",
///   "subsystems": [
///     {"id": "cameras", "label": "Cameras",
///      "status": "ok", "detail": "2/2 online"},
///     ...
///   ]
/// }
/// ```
class SystemHealth {
  final String overall; // 'ok' | 'warn' | 'error'
  final List<SubsystemHealth> subsystems;
  final DateTime timestamp;

  const SystemHealth({
    required this.overall,
    required this.subsystems,
    required this.timestamp,
  });

  factory SystemHealth.fromJson(Map<String, dynamic> json) {
    return SystemHealth(
      overall: json['overall']?.toString() ?? 'ok',
      subsystems: ((json['subsystems'] as List?) ?? [])
          .whereType<Map>()
          .map((e) =>
              SubsystemHealth.fromJson(e.cast<String, dynamic>()))
          .toList(),
      timestamp: DateTime.fromMillisecondsSinceEpoch(
        ((json['timestamp'] ?? 0) * 1000).toInt(),
      ),
    );
  }
}

class SubsystemHealth {
  final String id;
  final String label;
  final String status; // 'ok' | 'warn' | 'error'
  final String detail;

  const SubsystemHealth({
    required this.id,
    required this.label,
    required this.status,
    required this.detail,
  });

  factory SubsystemHealth.fromJson(Map<String, dynamic> json) {
    return SubsystemHealth(
      id:     json['id']?.toString() ?? '',
      label:  json['label']?.toString() ?? '',
      status: json['status']?.toString() ?? 'ok',
      detail: json['detail']?.toString() ?? '',
    );
  }
}
