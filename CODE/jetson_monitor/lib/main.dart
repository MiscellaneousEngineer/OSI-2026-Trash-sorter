import 'package:flutter/material.dart';
import 'screens/home_screen.dart';

void main() {
  runApp(const JetsonMonitorApp());
}

class JetsonMonitorApp extends StatelessWidget {
  const JetsonMonitorApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Jetson Monitor',
      debugShowCheckedModeBanner: false,
      theme: ThemeData.dark().copyWith(
        scaffoldBackgroundColor: const Color(0xFF0D0D0D),
        colorScheme: const ColorScheme.dark(primary: Colors.greenAccent),
      ),
      home: const HomeScreen(),
    );
  }
}