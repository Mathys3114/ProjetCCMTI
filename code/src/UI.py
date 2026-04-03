import os
import subprocess
import tkinter as tk
from tkinter import filedialog, messagebox
from PIL import Image, ImageTk #faut etre dans l'env python ça marche mieux

def resize_keep_aspect(img, max_size=400):
    w, h = img.size
    scale = min(max_size / w, max_size / h)
    new_w = max(1, int(w * scale))
    new_h = max(1, int(h * scale))
    return img.resize((new_w, new_h), Image.LANCZOS)

def open_image():
    path = filedialog.askopenfilename(
        filetypes=[("PPM files", "*.ppm"), ("All images", "*.*")]
    )
    if not path:
        return
    app_state["input_path"] = path
    img = Image.open(path)
    img = resize_keep_aspect(img, 400)
    app_state["photo_in"] = ImageTk.PhotoImage(img)
    label_in.config(image=app_state["photo_in"])
    label_in.image = app_state["photo_in"]

    orig_size = os.path.getsize(path)
    # Convertir en PNG pour afficher vraie taille compressée
    in_png_path = os.path.join(os.getcwd(), "in_original.png")
    img_temp = Image.open(path)
    img_temp.save(in_png_path, "PNG")
    in_png_size = os.path.getsize(in_png_path)
    label_size.config(text=f"PNG original : {in_png_size} octets | compressé : -")

def run_slic():
    in_path = app_state.get("input_path")
    if not in_path:
        messagebox.showerror("Erreur", "Aucune image sélectionnée")
        return
    k = int(entry_k.get() or 100)
    compactness = float(entry_compactness.get() or 10.0)
    out_path = os.path.join(os.getcwd(), "out_slic.ppm")

    exe = "./code/bin/SLIC"
    g = float(entry_g.get() or 2.0)
    cmd = [exe, str(k), str(compactness), str(g), in_path, out_path]
    try:
        subprocess.run(cmd, check=True)
        result = subprocess.run(["./code/bin/PSNR", in_path, out_path], capture_output=True, text=True, check=True)
        psnr_text = result.stdout.strip()
        try:
            psnr_value = psnr_text.split('=')[1].split('dB')[0].strip()
        except Exception:
            psnr_value = psnr_text
        psnr_label.config(text=f"PSNR : {psnr_value}")
    except subprocess.CalledProcessError as e:
        messagebox.showerror("Erreur", f"Echec SLIC:\n{e}")
        return

    if os.path.exists(out_path):
        img = Image.open(out_path)
        img = resize_keep_aspect(img, 400)
        app_state["photo_out"] = ImageTk.PhotoImage(img)
        label_out.config(image=app_state["photo_out"])
        label_out.image = app_state["photo_out"]

        # Conversion en PNG pour afficher vraies tailles compressées
        in_png_path = os.path.join(os.getcwd(), "in_original.png")
        out_png_path = os.path.join(os.getcwd(), "out_slic.png")
        
        img_orig = Image.open(in_path)
        img_orig.save(in_png_path, "PNG")
        
        img_comp = Image.open(out_path)
        img_comp.save(out_png_path, "PNG")

        in_png_size = os.path.getsize(in_png_path)
        out_png_size = os.path.getsize(out_png_path)
        ratio = (1 - out_png_size / in_png_size) * 100 if in_png_size > 0 else 0
        
        label_size.config(text=f"PNG original : {in_png_size} octets | compressé : {out_png_size} octets ({ratio:.1f}% gain)")

        # messagebox.showinfo("Succès", f"Sortie: {out_path}")
    else:
        messagebox.showerror("Erreur", "Fichier de sortie introuvable")

root = tk.Tk()
root.title("SLIC GUI Demo")
app_state = {}

frame = tk.Frame(root); frame.pack(padx=10, pady=10)
tk.Button(frame, text="Ouvrir image", command=open_image).grid(row=0, column=0, padx=4)
tk.Label(frame, text="k").grid(row=1, column=0, sticky="e")
entry_k = tk.Entry(frame, width=6); entry_k.insert(0, "200"); entry_k.grid(row=1, column=1)
tk.Label(frame, text="Compactness").grid(row=2, column=0, sticky="e")
entry_compactness = tk.Entry(frame, width=6); entry_compactness.insert(0, "10.0"); entry_compactness.grid(row=2, column=1)
tk.Label(frame, text="g").grid(row=3, column=0, sticky="e")
entry_g = tk.Entry(frame, width=6); entry_g.insert(0, "2.0"); entry_g.grid(row=3, column=1)
tk.Button(frame, text="Lancer SLIC", command=run_slic).grid(row=4, column=0, columnspan=2, pady=8)

images_frame = tk.Frame(root)
images_frame.pack(pady=10)

in_frame = tk.Frame(images_frame)
in_frame.pack(side="left", padx=10)
label_in = tk.Label(in_frame, text="Avant")
label_in.pack()

out_frame = tk.Frame(images_frame)
out_frame.pack(side="left", padx=10)
label_out = tk.Label(out_frame, text="Après")
label_out.pack()

info_frame = tk.Frame(root)
info_frame.pack(pady=8)

psnr_label = tk.Label(info_frame, text="PSNR : -")
psnr_label.grid(row=0, column=0, sticky="w", padx=4, pady=2)

label_size = tk.Label(info_frame, text="Taille originale : - / compressée : -")
label_size.grid(row=1, column=0, sticky="w", padx=4, pady=2)

root.mainloop()

#python code/src/UI.py dans l'env python