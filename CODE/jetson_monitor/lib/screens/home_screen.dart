import 'package:flutter/material.dart';
import 'camera_screen.dart';
import 'sensors_screen.dart';
import 'detections_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});
  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  int _currentIndex = 0;
  // 👇 Change this to your Jetson IP
  final String jetsonIp = '192.168.1.100';

  @override
  Widget build(BuildContext context) {
    final screens = [
      CameraScreen(jetsonIp: jetsonIp),
      const SensorsScreen(),
      const DetectionsScreen(),
    ];

    return Scaffold(
      body: screens[_currentIndex],
      bottomNavigationBar: BottomNavigationBar(
        backgroundColor: const Color(0xFF0D0D0D),
        selectedItemColor: Colors.greenAccent,
        unselectedItemColor: Colors.white38,
        currentIndex: _currentIndex,
        onTap: (i) => setState(() => _currentIndex = i),
        items: const [
          BottomNavigationBarItem(
              icon: Icon(Icons.videocam), label: 'Camera'),
          BottomNavigationBarItem(
              icon: Icon(Icons.sensors), label: 'Sensors'),
          BottomNavigationBarItem(
              icon: Icon(Icons.list_alt), label: 'Detections'),
        ],
      ),
    );
  }
}