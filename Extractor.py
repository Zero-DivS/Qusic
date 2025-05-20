import serial
import time
from midiutil import MIDIFile
import os
import tkinter as tk
from tkinter import filedialog
import sys

def convert_characters_to_midi(char_string, output_filename="output.mid", tempo=120, instrument=73):  # 73 is the MIDI number for flute
    """
    Convert a string of characters 1-9 and A-I to a MIDI file using flute notes.
    
    Parameters:
    char_string (str): String containing digits 1-9 and letters A-I
    output_filename (str): Output MIDI filename
    tempo (int): Tempo in BPM (beats per minute)
    instrument (int): MIDI instrument number (default: 73 for flute)
    """
    # Clean the input string to only contain digits 1-9 and letters A-I (case insensitive)
    valid_chars = []
    for char in char_string:
        if char.isdigit() and char != '0':
            valid_chars.append(char)
        elif char.upper() in 'ABCDEFGHI':
            valid_chars.append(char.upper())
    
    if not valid_chars:
        print("No valid characters (1-9, A-I) found in the input string.")
        return None
    
    # Create a MIDI file with one track
    midi = MIDIFile(1)
    
    # Setup the track
    track = 0
    channel = 0
    time = 0  # Start at the beginning
    duration = 0.5  # Each note is half a beat
    volume = 100  # 0-127
    
    # Add track name and tempo
    midi.addTrackName(track, time, "Flute Scale from Characters")
    midi.addTempo(track, time, tempo)
    
    # Set instrument to flute (program change)
    midi.addProgramChange(track, channel, time, instrument)
    
    # Map 1-9 and A-I to the flute notes (expanded range)
    # MIDI note numbers reference: C4 is 60, each semitone up is +1
    note_mapping = {    
        # Original mapping for digits 1-9
        '1': 83,  # B5
        '2': 82,  # A#5/Bb5
        '3': 81,  # A5
        '4': 79,  # G5
        '5': 78,  # F#5/Gb5
        '6': 77,  # F5
        '7': 76,  # E5
        '8': 74,  # D5
        '9': 75,  # D#5/Eb5
        
        # New mapping for letters A-I (extending the range to lower octaves)
        'A': 72,  # C5
        'B': 71,  # B4
        'C': 70,  # A#4/Bb4
        'D': 69,  # A4
        'E': 67,  # G4
        'F': 66,  # F#4/Gb4
        'G': 65,  # F4
        'H': 64,  # E4
        'I': 62   # D4
    }
    
    # Add notes to the track
    for i, char in enumerate(valid_chars):
        if char in note_mapping:
            pitch = note_mapping[char]
            midi.addNote(track, channel, pitch, time + i * duration, duration, volume)
    
    # Write the MIDI file
    with open(output_filename, "wb") as output_file:
        midi.writeFile(output_file)
    
    print(f"MIDI file '{output_filename}' created successfully.")
    print(f"Converted {len(valid_chars)} characters to flute notes.")
    return valid_chars

def read_from_serial_port(port='COM10', baud_rate=230400, read_duration=30, output_filename=None):
    """
    Read data from serial port for a specified duration and convert to MIDI.
    Provides regular updates and opens a save dialog.
    
    Parameters:
    port (str): Serial port name (e.g., 'COM10')
    baud_rate (int): Baud rate
    read_duration (int): How long to read from the serial port in seconds (default: 30)
    output_filename (str): Output MIDI filename or None to prompt for save location
    """
    try:
        # Configure and open serial port
        ser = serial.Serial(port, baud_rate, timeout=1)
        print(f"Connected to {port} at {baud_rate} baud rate.")
        print(f"Reading data for {read_duration} seconds...")
        
        # Clear any initial data
        ser.reset_input_buffer()
        
        # Read data for specified duration
        start_time = time.time()
        collected_data = ""
        update_interval = 1.0  # Update every 1 second
        next_update = start_time + update_interval
        
        chars_read = 0
        progress_bar_width = 40
        
        # Initial progress bar
        sys.stdout.write("[%s] 0%%" % (" " * progress_bar_width))
        sys.stdout.flush()
        sys.stdout.write("\r")
        
        while time.time() - start_time < read_duration:
            current_time = time.time()
            elapsed = current_time - start_time
            
            # Update progress bar
            if current_time >= next_update:
                percent_complete = int((elapsed / read_duration) * 100)
                progress = int(progress_bar_width * elapsed / read_duration)
                progress_bar = "#" * progress + " " * (progress_bar_width - progress)
                sys.stdout.write("[%s] %d%% (%d characters read)" % (progress_bar, percent_complete, chars_read))
                sys.stdout.flush()
                sys.stdout.write("\r")
                next_update = current_time + update_interval
            
            # Read data one byte at a time to capture each character
            if ser.in_waiting > 0:
                byte_data = ser.read(1)
                char_data = byte_data.decode('utf-8', errors='ignore')
                
                # Accept digits 1-9 and letters A-I (case insensitive)
                if (char_data.isdigit() and char_data != '0') or char_data.upper() in 'ABCDEFGHI':
                    collected_data += char_data.upper() if char_data.isalpha() else char_data
                    chars_read += 1
                
        # Final progress bar update
        sys.stdout.write("[%s] 100%% (%d characters read)\n" % ("#" * progress_bar_width, chars_read))
        sys.stdout.flush()
        
        # Close the serial connection
        ser.close()
        print("Serial port closed.")
        
        if not collected_data:
            print("No data received from serial port.")
            return None
            
        print(f"Data collection complete. {chars_read} characters collected.")
        
        # Create a hidden tkinter root window
        root = tk.Tk()
        root.withdraw()  # Hide the root window
        
        # If no output filename is provided, open a save dialog
        if output_filename is None:
            output_filename = filedialog.asksaveasfilename(
                defaultextension=".mid",
                filetypes=[("MIDI Files", "*.mid"), ("All Files", "*.*")],
                title="Save MIDI File"
            )
            
        # If user canceled the dialog, exit
        if not output_filename:
            print("Save operation canceled.")
            return None
            
        # Convert the collected data to MIDI
        print(f"Converting {chars_read} characters to MIDI file: {output_filename}")
        chars = convert_characters_to_midi(collected_data, output_filename)
        
        # Open the folder containing the saved file
        if chars:
            folder_path = os.path.dirname(os.path.abspath(output_filename))
            print(f"Opening folder: {folder_path}")
            # Open file explorer to the directory containing the file
            os.startfile(folder_path) if sys.platform == 'win32' else os.system(f'open "{folder_path}"')
        
        return chars
        
    except serial.SerialException as e:
        print(f"Error: {e}")
        print("Make sure the Arduino is connected and the port is correct.")
        return None

if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description='Convert serial port characters (1-9, A-I) to flute MIDI')
    parser.add_argument('--port', default='COM10', help='Serial port (default: COM10)')
    parser.add_argument('--baud', type=int, default=230400, help='Baud rate (default: 230400)')
    parser.add_argument('--duration', type=int, default=30, help='Read duration in seconds (default: 30)')
    parser.add_argument('--tempo', type=int, default=120, help='Tempo in BPM (default: 120)')
    parser.add_argument('--instrument', type=int, default=73, help='MIDI instrument number (default: 73 - flute)')
    
    args = parser.parse_args()
    
    print("Serial to Flute MIDI Converter (1-9 & A-I)")
    print("------------------------------------------")
    read_from_serial_port(args.port, args.baud, args.duration)