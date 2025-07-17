import csv
import os
import re

# --- CONFIGURATION ---
# The exact fieldnames for the FMusicData struct in your Unreal Engine project.
# This MUST match your C++ struct definition precisely (case-sensitive).
CSV_HEADER = [
    # Unreal's required row name
    'Name',
    # Common Data
    'TimestampMS', 'EntryType',
    # Global Difficulty
    'HPDrainRate', 'CircleSize', 'OverallDifficulty', 'ApproachRate',
    # Hit Object Data
    'HitObjectType', 'HitSound', 'SliderEndTimeMS', 'Repeats', 'SliderTickRate',
    # Break Data
    'BreakEndTimeMS',
    # Timing Point Data
    'Uninherited', 'BeatLength', 'Meter', 'Effects',
    # Audio Data (not generated, will be 0)
    'AudioBeatStrength'
]

def sanitize_filename(name):
    """Removes characters that are invalid for filenames."""
    return re.sub(r'[\\/*?:"<>|]', "", name)

def parse_osu_file(filepath):
    """
    Parses a single .osu file into a structured dictionary containing all
    relevant sections for our data conversion.
    """
    data = {
        'General': {},
        'Metadata': {},
        'Difficulty': {},
        'Events': [],
        'TimingPoints': [],
        'HitObjects': []
    }
    current_section = ""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('//'):
                    continue
                if line.startswith('['):
                    current_section = line.strip('[]')
                    continue

                if ':' in line and current_section in ['General', 'Metadata', 'Difficulty']:
                    key, value = map(str.strip, line.split(':', 1))
                    data[current_section][key] = value
                elif current_section in ['Events', 'TimingPoints', 'HitObjects']:
                    data[current_section].append(line.split(','))

    except Exception as e:
        print(f"  - WARNING: Could not fully parse {os.path.basename(filepath)}. Error: {e}")
    
    return data

def process_to_fmusicdata(parsed_data):
    """
    Converts the raw parsed data from an .osu file into a list of FMusicData-compliant
    dictionaries, ready to be written to a CSV.
    """
    all_events = []
    
    difficulty_section = parsed_data['Difficulty']
    hp_drain_rate = float(difficulty_section.get('HPDrainRate', 5.0))
    circle_size = float(difficulty_section.get('CircleSize', 5.0))
    overall_difficulty = float(difficulty_section.get('OverallDifficulty', 5.0))
    approach_rate = float(difficulty_section.get('ApproachRate', 5.0))
    slider_tick_rate = float(difficulty_section.get('SliderTickRate', 1.0))
    slider_multiplier = float(difficulty_section.get('SliderMultiplier', 1.4))

    difficulty_data = {
        'HPDrainRate': hp_drain_rate,
        'CircleSize': circle_size,
        'OverallDifficulty': overall_difficulty,
        'ApproachRate': approach_rate
    }
    
    master_timing_points = []
    for parts in parsed_data['TimingPoints']:
        if len(parts) < 2: continue
        time = int(float(parts[0]))
        beat_length = float(parts[1])
        uninherited = 1 if beat_length > 0 else 0

        master_timing_points.append({
            'Time': time, 'BeatLength': beat_length,
            'Meter': int(parts[2]) if len(parts) > 2 else 4,
            'Uninherited': uninherited,
            'Effects': int(parts[7]) if len(parts) > 7 else 0
        })
        
        # NOTE: For your enum, Unreal will map "Timing Point" to EGameplayEntryType::TimingPoint
        timing_event = {
            'EntryType': 'Timing Point', 
            'TimestampMS': time,
            # --- FIX: Use the beat_length value directly. ---
            # For uninherited points, this is ms_per_beat.
            # For inherited points, this is already the correct -100/velocity_multiplier.
            'BeatLength': beat_length,
            'Meter': int(parts[2]) if len(parts) > 2 else 4,
            'Uninherited': uninherited,
            'Effects': int(parts[7]) if len(parts) > 7 else 0
        }
        all_events.append({**timing_event, **difficulty_data})

    for parts in parsed_data['Events']:
        if parts[0] == '2' or (parts[0].lower() == 'break' and len(parts) > 2):
            break_event = {
                'EntryType': 'Break',
                'TimestampMS': int(parts[1]),
                'BreakEndTimeMS': int(parts[2])
            }
            all_events.append({**break_event, **difficulty_data})
    
    tp_idx = 0
    base_beat_length = 500
    for tp in master_timing_points:
        if tp['Uninherited'] == 1:
            base_beat_length = tp['BeatLength']
            break

    for ho_parts in parsed_data['HitObjects']:
        if len(ho_parts) < 4: continue
        timestamp = int(ho_parts[2])
        
        while tp_idx + 1 < len(master_timing_points) and timestamp >= master_timing_points[tp_idx+1]['Time']:
            tp_idx += 1
        active_tp = master_timing_points[tp_idx]
        
        if active_tp['Uninherited'] == 0:
            temp_base_beat_length = base_beat_length
            for i in range(tp_idx, -1, -1):
                if master_timing_points[i]['Uninherited'] == 1:
                    temp_base_beat_length = master_timing_points[i]['BeatLength']
                    break
            slider_velocity_multiplier = -100.0 / active_tp['BeatLength'] if active_tp['BeatLength'] < 0 else 1.0
            current_beat_length_for_slider = temp_base_beat_length / slider_velocity_multiplier
        else:
            base_beat_length = active_tp['BeatLength']
            current_beat_length_for_slider = base_beat_length
        
        obj_type = int(ho_parts[3])
        hit_object_event = {
            'EntryType': 'Hit Object',
            'TimestampMS': timestamp,
            'HitObjectType': obj_type,
            'HitSound': int(ho_parts[4]),
            'SliderTickRate': slider_tick_rate
        }

        is_slider = obj_type & 2
        if is_slider and len(ho_parts) > 7:
            repeats = int(ho_parts[6])
            pixel_length = float(ho_parts[7])
            slider_duration_ms = (pixel_length / (slider_multiplier * 100.0)) * current_beat_length_for_slider
            hit_object_event['Repeats'] = repeats
            hit_object_event['SliderEndTimeMS'] = timestamp + int(slider_duration_ms * repeats)

        all_events.append({**hit_object_event, **difficulty_data})
        
    all_events.sort(key=lambda x: x['TimestampMS'])
    return all_events

def generate_datatables_for_folder(folder_path):
    osu_files = [f for f in os.listdir(folder_path) if f.lower().endswith(".osu")]
    if not osu_files:
        print("FATAL: No .osu files found in this folder.")
        return

    print(f"Found {len(osu_files)} difficulties. Starting conversion to CSV DataTables...")

    for osu_filename in osu_files:
        filepath = os.path.join(folder_path, osu_filename)
        print(f"\n--- Processing: {osu_filename} ---")

        parsed_data = parse_osu_file(filepath)
        if not parsed_data['HitObjects'] or not parsed_data['TimingPoints']:
            print("  - SKIPPING: File seems empty or invalid.")
            continue

        fmusicdata_events = process_to_fmusicdata(parsed_data)

        song_title = parsed_data['Metadata'].get('Title', 'Song')
        diff_name = parsed_data['Metadata'].get('Version', os.path.splitext(osu_filename)[0])
        
        output_filename = f"{sanitize_filename(song_title)}_{sanitize_filename(diff_name)}.csv"
        output_path = os.path.join(folder_path, output_filename)
        
        print(f"  - Generating DataTable -> {output_filename}")
        with open(output_path, 'w', newline='', encoding='utf-8') as csvfile:
            writer = csv.DictWriter(csvfile, fieldnames=CSV_HEADER, restval='0')
            writer.writeheader()
            
            row_index = 0
            for row_data in fmusicdata_events:
                row_data['Name'] = f"Row_{row_index}"
                writer.writerow(row_data)
                row_index += 1
    
    print("\n--- CONVERSION COMPLETE ---")
    print("Import the generated .csv files into Unreal Engine as DataTables.")
    print("Make sure to select FMusicData as the row structure during import.")

if __name__ == "__main__":
    script_dir = os.path.dirname(os.path.abspath(__file__))
    generate_datatables_for_folder(script_dir)