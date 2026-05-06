import 'dart:async';

import 'package:flutter/material.dart';
import '../config.dart';

/// Polls the Jetson's `/camera` endpoint on a Timer to fake a video feed.
///
/// We use a polling approach (not MJPEG) because Flutter's `Image.network`
/// can't decode `multipart/x-mixed-replace`, and adding a dedicated MJPEG
/// package isn't worth the dep for a demo. The Jetson's MJPEG stream is
/// still available at `/camera/stream` for browser debugging.
class CameraScreen extends StatefulWidget {
  const CameraScreen({super.key});

  @override
  State<CameraScreen> createState() => _CameraScreenState();
}

class _CameraScreenState extends State<CameraScreen> {
  static const Duration _refreshRate = Duration(milliseconds: 100); // ~10 fps
  Timer? _timer;
  int _ts = DateTime.now().millisecondsSinceEpoch;

  @override
  void initState() {
    super.initState();
    _timer = Timer.periodic(_refreshRate, (_) {
      if (mounted) {
        setState(() => _ts = DateTime.now().millisecondsSinceEpoch);
      }
    });
  }

  @override
  void dispose() {
    _timer?.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: Colors.black,
      appBar: AppBar(
        title: const Text('Live Camera'),
        backgroundColor: Colors.black,
        foregroundColor: Colors.greenAccent,
      ),
      body: Center(
        child: Image.network(
          // Cache-busting query param forces Flutter to refetch every tick.
          '${AppConfig.httpBase}/camera?t=$_ts',
          fit: BoxFit.contain,
          gaplessPlayback: true, // keep showing old frame while next loads
          loadingBuilder: (context, child, progress) {
            if (progress == null) return child;
            return const CircularProgressIndicator(color: Colors.greenAccent);
          },
          errorBuilder: (context, error, stack) => const Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Icon(Icons.videocam_off, color: Colors.red, size: 60),
              SizedBox(height: 12),
              Text('Camera unavailable',
                  style: TextStyle(color: Colors.white)),
            ],
          ),
        ),
      ),
    );
  }
}
