import 'package:flutter/material.dart';

class CameraScreen extends StatelessWidget {
  final String jetsonIp;
  const CameraScreen({super.key, required this.jetsonIp});

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
          'http://$jetsonIp:8000/camera',
          fit: BoxFit.contain,
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