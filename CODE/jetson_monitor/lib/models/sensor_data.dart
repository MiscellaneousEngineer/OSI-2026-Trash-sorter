class SensorData {
  final double temperature;
  final double gpuUsage;
  final double cpuUsage;
  final Map<String, double> imu;
  final DateTime timestamp;

  SensorData({
    required this.temperature,
    required this.gpuUsage,
    required this.cpuUsage,
    required this.imu,
    required this.timestamp,
  });

  factory SensorData.fromJson(Map<String, dynamic> json) {
    return SensorData(
      temperature: (json['temperature'] ?? 0).toDouble(),
      gpuUsage:    (json['gpu_usage'] ?? 0).toDouble(),
      cpuUsage:    (json['cpu_usage'] ?? 0).toDouble(),
      imu:         Map<String, double>.from(json['imu'] ?? {}),
      timestamp:   DateTime.fromMillisecondsSinceEpoch(
                     ((json['timestamp'] ?? 0) * 1000).toInt()),
    );
  }
}