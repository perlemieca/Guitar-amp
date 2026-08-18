package com.example.guitarampengine

import android.Manifest
import android.content.pm.PackageManager
import android.os.Bundle
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import com.google.android.material.button.MaterialButton
import com.google.android.material.slider.Slider

class MainActivity : AppCompatActivity() {

    private var engineRunning = false
    private lateinit var startStopButton: MaterialButton
    private lateinit var statusText: TextView

    private val requestMicPermission =
        registerForActivityResult(
            androidx.activity.result.contract.ActivityResultContracts.RequestPermission()
        ) { granted ->
            if (granted) {
                toggleEngine()
            } else {
                Toast.makeText(
                    this,
                    "Il permesso microfono è necessario per usare l'ampli.",
                    Toast.LENGTH_LONG
                ).show()
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        startStopButton = findViewById(R.id.startStopButton)
        statusText = findViewById(R.id.statusText)

        val gainSlider = findViewById<Slider>(R.id.gainSlider)
        val toneSlider = findViewById<Slider>(R.id.toneSlider)
        val volumeSlider = findViewById<Slider>(R.id.volumeSlider)

        gainSlider.value = 0.21f
        toneSlider.value = 0.5f
        volumeSlide
