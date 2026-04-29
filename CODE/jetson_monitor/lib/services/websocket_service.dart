import 'dart:convert';
import 'package:web_socket_channel/web_socket_channel.dart';
import '../config.dart';

class WebSocketService {
  WebSocketChannel? _detectionChannel;
  WebSocketChannel? _binChannel;

  /// Object detections coming from the YOLO inference loop on the Jetson.
  Stream<Map<String, dynamic>> get detectionStream {
    _detectionChannel = WebSocketChannel.connect(
      Uri.parse('${AppConfig.wsBase}/detections'),
    );
    return _detectionChannel!.stream
        .map((data) => jsonDecode(data) as Map<String, dynamic>);
  }

  /// Bin fill levels.
  ///
  /// Expected JSON payload:
  /// ```json
  /// {
  ///   "timestamp": 1714400000.123,
  ///   "bins": [
  ///     {"id": "metal",   "label": "Metal",   "fill_percent": 73.5, "capacity_l": 20},
  ///     {"id": "glass",   "label": "Glass",   "fill_percent": 45.0, "capacity_l": 20},
  ///     {"id": "plastic", "label": "Plastic", "fill_percent": 12.0, "capacity_l": 20},
  ///     {"id": "paper",   "label": "Paper",   "fill_percent": 88.0, "capacity_l": 20},
  ///     {"id": "other",   "label": "Other",   "fill_percent": 30.0, "capacity_l": 20}
  ///   ]
  /// }
  /// ```
  Stream<Map<String, dynamic>> get binStream {
    _binChannel = WebSocketChannel.connect(
      Uri.parse('${AppConfig.wsBase}/bins'),
    );
    return _binChannel!.stream
        .map((data) => jsonDecode(data) as Map<String, dynamic>);
  }

  void dispose() {
    _detectionChannel?.sink.close();
    _binChannel?.sink.close();
  }
}
