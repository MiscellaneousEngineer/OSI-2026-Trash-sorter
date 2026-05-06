class Detection {
  final int id;
  final String label;
  final double confidence;
  final List<int> bbox;
  final DateTime timestamp;

  Detection({
    required this.id,
    required this.label,
    required this.confidence,
    required this.bbox,
    required this.timestamp,
  });

  factory Detection.fromJson(Map<String, dynamic> json, DateTime timestamp) {
    return Detection(
      id:         json['id'] ?? 0,
      label:      json['label'] ?? '',
      confidence: (json['confidence'] ?? 0).toDouble(),
      bbox:       List<int>.from(json['bbox'] ?? []),
      timestamp:  timestamp,
    );
  }
}