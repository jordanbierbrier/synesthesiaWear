# Synesthesia Wear Source Code

This folder contains the main source code for the **Synesthesia Wear** system, organized into the following submodules:

---

## 📁 Folders

- `inferencing/`:  
  Contains scripts and resources for running real-time audio inference using the trained machine learning model.  
  It handles feature extraction (e.g., MFCC) and passes data through the deployed 1D Convolutional Neural Network (1D-CNN) for sound classification.

- `proofOfConcept/`:  
  Early prototype scripts demonstrating initial concepts such as basic audio capture, vibration testing, and simple signal processing before full system integration.

- `speechDetect/`:  
  Contains the Arduino code that runs on the embedded device (wearable bracelet).  
  This firmware handles:
  - Capturing audio signals via onboard microphone
  - Basic sound detection or preprocessing
  - Sending recognized events (e.g., detected keywords) over Bluetooth for triggering haptic feedback
