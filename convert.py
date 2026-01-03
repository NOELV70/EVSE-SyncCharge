from PIL import Image
import sys

def image_to_c_array(image_path, array_name="logo_data"):
    try:
        # Open the image
        img = Image.open(image_path)
        
        # Ensure it's in a standard format (RGBA if you want transparency)
        img = img.convert("RGBA")
        
        # Get raw bytes
        with open(image_path, "rb") as f:
            data = f.read()

        # Generate the C code
        print(f"// Image: {image_path}")
        print(f"// Size: {len(data)} bytes")
        print(f"const unsigned char {array_name}[] PROGMEM = {{")
        
        for i, byte in enumerate(data):
            print(f"0x{byte:02x}", end=", ")
            if (i + 1) % 12 == 0:
                print()  # New line every 12 bytes for readability
                
        print("\n};")
        print(f"const unsigned int {array_name}_len = {len(data)};")

    except Exception as e:
        print(f"Error: {e}")

# Usage: Change 'logo.png' to your filename
image_to_c_array("SEVSE_Image.png")