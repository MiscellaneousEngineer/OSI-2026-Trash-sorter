/// One sorting bin on the Jetson side.
///
/// Expected JSON shape:
/// ```json
/// {
///   "id": "metal",
///   "label": "Metal",
///   "fill_percent": 73.5,
///   "capacity_l": 20
/// }
/// ```
class BinStatus {
  final String id;
  final String label;
  final double fillPercent;
  final double capacityL;

  const BinStatus({
    required this.id,
    required this.label,
    required this.fillPercent,
    required this.capacityL,
  });

  factory BinStatus.fromJson(Map<String, dynamic> json) {
    return BinStatus(
      id:           json['id']?.toString() ?? '',
      label:        json['label']?.toString() ?? '',
      fillPercent:  (json['fill_percent'] ?? 0).toDouble().clamp(0.0, 100.0),
      capacityL:    (json['capacity_l'] ?? 0).toDouble(),
    );
  }

  /// Litres currently in the bin (derived).
  double get filledL => capacityL * fillPercent / 100;
}
