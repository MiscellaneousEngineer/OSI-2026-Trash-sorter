import 'package:web_socket_channel/web_socket_channel.dart';
import 'dart:convert';

class WebSocketService {
  // 👇 Change this to your Jetson IP
  final String host = '';

  WebSocketChannel? _sensorChannel;
  WebSocketChannel? _detectionChannel;

  Stream<Map<String, dynamic>> get sensorStream {
    _sensorChannel = WebSocketChannel.connect(
      Uri.parse('ws://$host:8000/sensors'),
    );
    return _sensorChannel!.stream
        .map((data) => jsonDecode(data) as Map<String, dynamic>);
  }

  Stream<Map<String, dynamic>> get detectionStream {
    _detectionChannel = WebSocketChannel.connect(
      Uri.parse('ws://$host:8000/detections'),
    );
    return _detectionChannel!.stream
        .map((data) => jsonDecode(data) as Map<String, dynamic>);
  }

  void dispose() {
    _sensorChannel?.sink.close();
    _detectionChannel?.sink.close();
  }
}