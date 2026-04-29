import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;
import '../config.dart';

/// Polls the Flask dashboard's REST endpoints at a fixed cadence.
class ApiService {
  Stream<Map<String, dynamic>> _poll(String path, Duration interval) async* {
    final url = Uri.parse('${AppConfig.httpBase}$path');
    while (true) {
      try {
        final resp = await http.get(url).timeout(const Duration(seconds: 4));
        if (resp.statusCode == 200) {
          yield jsonDecode(resp.body) as Map<String, dynamic>;
        }
      } catch (_) {
        // Server unreachable / timed out — silent retry next tick.
      }
      await Future.delayed(interval);
    }
  }

  Stream<Map<String, dynamic>> get binStream =>
      _poll('/bins', const Duration(seconds: 1));

  Stream<Map<String, dynamic>> get detectionStream =>
      _poll('/detections', const Duration(milliseconds: 500));

  Stream<Map<String, dynamic>> get healthStream =>
      _poll('/health', const Duration(seconds: 2));

  void dispose() {
    // Polling stops automatically when the StreamBuilder cancels.
  }
}
