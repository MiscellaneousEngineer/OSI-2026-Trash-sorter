import 'package:flutter/material.dart';
import 'camera_screen.dart';
import 'detections_screen.dart';
import 'bins_screen.dart';
import 'health_screen.dart';
import 'map_screen.dart';

class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});
  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  int _currentIndex = 0;

  @override
  Widget build(BuildContext context) {
    const screens = [
      CameraScreen(),
      DetectionsScreen(),
      BinsScreen(),
      HealthScreen(),
      MapScreen(),
    ];

    return Scaffold(
      body: screens[_currentIndex],
      bottomNavigationBar: BottomNavigationBar(
        type: BottomNavigationBarType.fixed,
        backgroundColor: const Color(0xFF0D0D0D),
        selectedItemColor: Colors.greenAccent,
        unselectedItemColor: Colors.white38,
        currentIndex: _currentIndex,
        onTap: (i) => setState(() => _currentIndex = i),
        items: const [
          BottomNavigationBarItem(
              icon: Icon(Icons.videocam), label: 'Camera'),
          BottomNavigationBarItem(
              icon: Icon(Icons.list_alt), label: 'Detections'),
          BottomNavigationBarItem(
              icon: Icon(Icons.delete_outline), label: 'Bins'),
          BottomNavigationBarItem(
              icon: Icon(Icons.health_and_safety_outlined), label: 'Health'),
          BottomNavigationBarItem(
              icon: Icon(Icons.map_outlined), label: 'Map'),
        ],
      ),
    );
  }
}
