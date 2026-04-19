import os
import re
import subprocess
import threading
import time
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
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
    label_size.config(text=f"PNG original : {in_png_size} octets | taux de compression (orig/compressé) : -")
    refresh_optimal_controls()

def refresh_optimal_controls():
    has_image = bool(app_state.get("input_path"))
    has_mode = mode_var.get() in ("SLIC", "slicCC")
    searching = app_state.get("searching_optimal", False)

    can_search = has_image and has_mode and (not searching)
    optimal_button.config(state="normal" if can_search else "disabled")
    cancel_optimal_button.config(state="normal" if searching else "disabled")

    if searching:
        progress_status_label.config(text="Recherche paramètres optimaux en cours...")
        optimal_progress.start(12)
    else:
        optimal_progress.stop()
        optimal_progress.configure(value=0)
        if not has_image:
            progress_status_label.config(text="Sélectionnez une image pour activer la recherche.")
        elif not has_mode:
            progress_status_label.config(text="Sélectionnez un mode pour activer la recherche.")
        else:
            progress_status_label.config(text="Prêt pour la recherche des paramètres optimaux.")

def on_mode_change():
    refresh_optimal_controls()

def get_selected_mode():
    mode = mode_var.get()
    if mode not in ("SLIC", "slicCC"):
        messagebox.showerror("Erreur", "Veuillez sélectionner un mode : SLIC ou slicCC")
        return None
    return mode

def build_command(mode, k, compactness, g, in_path, out_cc_path, out_path):
    if mode == "SLIC":
        return ["./code/bin/SLIC", str(k), str(compactness), str(g), in_path, out_path]
    return ["./code/bin/slicCC", str(k), str(compactness), str(g), in_path, out_cc_path, out_path]

def parse_psnr_output(psnr_text):
    try:
        return psnr_text.split('=')[1].split('dB')[0].strip()
    except Exception:
        return psnr_text

def compute_ui_metrics(mode, k, compactness, g, in_path, out_cc_path, out_path):
    cmd = build_command(mode, k, compactness, g, in_path, out_cc_path, out_path)
    subprocess.run(cmd, check=True)

    psnr_result = subprocess.run(
        ["./code/bin/PSNR", in_path, out_path],
        capture_output=True,
        text=True,
        check=True,
    )
    psnr_value = parse_psnr_output(psnr_result.stdout.strip())

    # Conversion en PNG pour un taux de compression cohérent avec l'affichage UI.
    in_png_path = os.path.join(os.getcwd(), "in_original.png")
    out_png_path = os.path.join(os.getcwd(), "out_slic.png")

    img_orig = Image.open(in_path)
    img_orig.save(in_png_path, "PNG")

    # Pour slicCC, mesurer la compression réelle (out_cc), pas out_clean
    comp_file_to_measure = out_cc_path if mode == "slicCC" else out_path
    img_comp = Image.open(comp_file_to_measure)
    img_comp.save(out_png_path, "PNG")

    in_png_size = os.path.getsize(in_png_path)
    out_png_size = os.path.getsize(out_png_path)
    compression_rate = (in_png_size / out_png_size) if out_png_size > 0 else 0

    return {
        "psnr_value": psnr_value,
        "in_png_size": in_png_size,
        "out_png_size": out_png_size,
        "compression_rate": compression_rate,
    }

def parse_optimal_parameters(text):
    num_pattern = r"([+-]?\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)"
    match_k = re.search(r"-\s*K\s*:\s*" + num_pattern, text)
    match_g = re.search(r"-\s*g\s*:\s*" + num_pattern, text)
    match_m = re.search(r"-\s*m\s*:\s*" + num_pattern, text)
    match_psnr = re.search(r"-\s*PSNR\s*:\s*" + num_pattern, text)
    match_cf = re.search(r"-\s*Compression factor\s*:\s*" + num_pattern, text)

    if not (match_k and match_g and match_m):
        return None

    params = {
        "k": float(match_k.group(1)),
        "g": float(match_g.group(1)),
        "m": float(match_m.group(1)),
    }
    if match_psnr:
        params["psnr"] = float(match_psnr.group(1))
    if match_cf:
        params["internal_cf"] = float(match_cf.group(1))
    return params

def update_optimal_label():
    params = app_state.get("last_optimal_params")
    if not params:
        optimal_label.config(text="Paramètres optimaux : -")
        return

    mode = params["mode"]
    applied = "oui" if params.get("applied") else "non"
    psnr_txt = f" | PSNR={params['psnr']:.2f}" if "psnr" in params else ""
    cf_txt = f" | taux={params['cf']:.2f} (orig/compressé)" if "cf" in params else ""
    optimal_label.config(
        text=(
            f"Derniers paramètres optimaux ({mode}) : "
            f"K={params['k']:.2f}, m={params['m']:.3f}, g={params['g']:.2f}"
            f"{psnr_txt}{cf_txt} | appliqués : {applied}"
        )
    )

def find_optimal_parameters():
    in_path = app_state.get("input_path")
    if not in_path:
        messagebox.showerror("Erreur", "Aucune image sélectionnée")
        return

    mode = get_selected_mode()
    if mode is None:
        return

    messagebox.showinfo(
        "Information",
        "Recherche des paramètres optimaux en cours.\n"
        "Cela peut prendre un peu de temps selon la taille et le contenu de l'image."
    )

    app_state["searching_optimal"] = True
    app_state["cancel_optimal"] = False
    app_state["optimal_process"] = None
    refresh_optimal_controls()

    auto_out_path = os.path.join(os.getcwd(), f"best_{mode}.slic")

    def worker():
        try:
            process = subprocess.Popen(
                ["./code/bin/autoSLIC", mode, in_path, auto_out_path],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            app_state["optimal_process"] = process

            while process.poll() is None:
                if app_state.get("cancel_optimal", False):
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                    process.communicate()
                    root.after(0, lambda: on_optimal_search_cancelled(mode))
                    return
                time.sleep(0.1)

            stdout_text, stderr_text = process.communicate()
            if app_state.get("cancel_optimal", False):
                root.after(0, lambda: on_optimal_search_cancelled(mode))
                return

            if process.returncode != 0:
                err = subprocess.CalledProcessError(
                    process.returncode,
                    ["./code/bin/autoSLIC", mode, in_path, auto_out_path],
                    output=stdout_text,
                    stderr=stderr_text,
                )
                root.after(0, lambda: on_optimal_search_complete(mode, None, err, stderr_text))
                return

            parsed = parse_optimal_parameters(stdout_text)
            root.after(0, lambda: on_optimal_search_complete(mode, parsed, None, stderr_text))
        except Exception as e:
            root.after(0, lambda: on_optimal_search_complete(mode, None, e, ""))

    threading.Thread(target=worker, daemon=True).start()

def cancel_optimal_search():
    if not app_state.get("searching_optimal", False):
        return
    app_state["cancel_optimal"] = True
    progress_status_label.config(text="Annulation de la recherche en cours...")
    process = app_state.get("optimal_process")
    if process is not None and process.poll() is None:
        try:
            process.terminate()
        except Exception:
            pass

def on_optimal_search_cancelled(mode):
    app_state["searching_optimal"] = False
    app_state["cancel_optimal"] = False
    app_state["optimal_process"] = None
    refresh_optimal_controls()
    messagebox.showinfo("Recherche annulée", f"Recherche des paramètres optimaux annulée pour le mode {mode}.")

def on_optimal_search_complete(mode, parsed, error, stderr_text):
    app_state["searching_optimal"] = False
    app_state["cancel_optimal"] = False
    app_state["optimal_process"] = None
    refresh_optimal_controls()

    if error is not None:
        messagebox.showerror("Erreur", f"Echec recherche paramètres optimaux :\n{error}\n{stderr_text or ''}")
        return

    if not parsed:
        messagebox.showerror(
            "Erreur",
            "Impossible de lire les paramètres optimaux dans la sortie de autoSLIC."
        )
        return

    app_state["last_optimal_params"] = {
        "mode": mode,
        "k": parsed["k"],
        "m": parsed["m"],
        "g": parsed["g"],
        "psnr": None,
        "cf": None,
        "internal_psnr": parsed.get("psnr"),
        "internal_cf": parsed.get("internal_cf"),
        "applied": False,
    }

    # Recalcule PSNR+taux avec la meme logique que le bouton "Lancer".
    in_path = app_state.get("input_path")
    out_cc_path = os.path.join(os.getcwd(), "out_slic_cc.ppm")
    out_path = os.path.join(os.getcwd(), "out_slic_clean.ppm")
    try:
        measured = compute_ui_metrics(mode, parsed["k"], parsed["m"], parsed["g"], in_path, out_cc_path, out_path)
        app_state["last_optimal_params"]["psnr"] = float(measured["psnr_value"])
        app_state["last_optimal_params"]["cf"] = measured["compression_rate"]
    except Exception:
        # Si la mesure exacte echoue, on garde les valeurs internes autoSLIC en secours.
        app_state["last_optimal_params"]["psnr"] = parsed.get("psnr")
        app_state["last_optimal_params"]["cf"] = parsed.get("internal_cf")

    update_optimal_label()

    apply_now = messagebox.askyesno(
        "Paramètres optimaux trouvés",
        (
            f"Mode : {mode}\n"
            f"K={parsed['k']:.2f}, m={parsed['m']:.3f}, g={parsed['g']:.2f}\n\n"
            "Voulez-vous appliquer ces paramètres maintenant ?"
        )
    )

    if apply_now:
        entry_k.delete(0, tk.END)
        entry_k.insert(0, f"{parsed['k']:.6g}")
        entry_compactness.delete(0, tk.END)
        entry_compactness.insert(0, f"{parsed['m']:.6g}")
        entry_g.delete(0, tk.END)
        entry_g.insert(0, f"{parsed['g']:.6g}")
        app_state["last_optimal_params"]["applied"] = True
        update_optimal_label()

def run_slic():
    in_path = app_state.get("input_path")
    if not in_path:
        messagebox.showerror("Erreur", "Aucune image sélectionnée")
        return

    mode = get_selected_mode()
    if mode is None:
        return

    try:
        k = float(entry_k.get() or 100)
        compactness = float(entry_compactness.get() or 10.0)
        g = float(entry_g.get() or 2.0)
    except ValueError:
        messagebox.showerror("Erreur", "Paramètres invalides (K, m, g)")
        return

    out_cc_path = os.path.join(os.getcwd(), "out_slic_cc.ppm")
    out_path = os.path.join(os.getcwd(), "out_slic_clean.ppm")

    try:
        measured = compute_ui_metrics(mode, k, compactness, g, in_path, out_cc_path, out_path)
        psnr_value = measured["psnr_value"]
        psnr_label.config(text=f"PSNR : {psnr_value}")
        mode_label.config(text=f"Mode actif : {mode}")
    except subprocess.CalledProcessError as e:
        messagebox.showerror("Erreur", f"Echec SLIC:\n{e}")
        return

    if os.path.exists(out_path):
        img = Image.open(out_path)
        img = resize_keep_aspect(img, 400)
        app_state["photo_out"] = ImageTk.PhotoImage(img)
        label_out.config(image=app_state["photo_out"])
        label_out.image = app_state["photo_out"]

        in_png_size = measured["in_png_size"]
        out_png_size = measured["out_png_size"]
        compression_factor = measured["compression_rate"]
        
        label_size.config(text=f"PNG original : {in_png_size} octets | compressé : {out_png_size} octets | taux de compression (orig/compressé) : {compression_factor:.2f}")

        # messagebox.showinfo("Succès", f"Sortie: {out_path}")
    else:
        messagebox.showerror("Erreur", "Fichier de sortie introuvable")

root = tk.Tk()
root.title("SLIC GUI Demo")
app_state = {}

frame = tk.Frame(root); frame.pack(padx=10, pady=10)
tk.Button(frame, text="Ouvrir image", command=open_image).grid(row=0, column=0, padx=4)
mode_var = tk.StringVar(value="")
tk.Label(frame, text="Mode").grid(row=1, column=0, sticky="e")
mode_frame = tk.Frame(frame)
mode_frame.grid(row=1, column=1, sticky="w")
tk.Radiobutton(mode_frame, text="SLIC classique", variable=mode_var, value="SLIC", command=on_mode_change).pack(side="left")
tk.Radiobutton(mode_frame, text="slicCC", variable=mode_var, value="slicCC", command=on_mode_change).pack(side="left")

tk.Label(frame, text="K").grid(row=2, column=0, sticky="e")
entry_k = tk.Entry(frame, width=8); entry_k.insert(0, "200"); entry_k.grid(row=2, column=1, sticky="w")
tk.Label(frame, text="m").grid(row=3, column=0, sticky="e")
entry_compactness = tk.Entry(frame, width=8); entry_compactness.insert(0, "10.0"); entry_compactness.grid(row=3, column=1, sticky="w")
tk.Label(frame, text="g").grid(row=4, column=0, sticky="e")
entry_g = tk.Entry(frame, width=8); entry_g.insert(0, "2.0"); entry_g.grid(row=4, column=1, sticky="w")

optimal_button = tk.Button(frame, text="Trouver paramètres optimaux", command=find_optimal_parameters, state="disabled")
optimal_button.grid(row=5, column=0, columnspan=2, pady=6)
cancel_optimal_button = tk.Button(frame, text="Annuler la recherche", command=cancel_optimal_search, state="disabled")
cancel_optimal_button.grid(row=6, column=0, columnspan=2, pady=(0, 4))
tk.Label(
    frame,
    text="Cette recherche peut prendre du temps selon l'image.",
    fg="#444"
).grid(row=7, column=0, columnspan=2)

optimal_progress = ttk.Progressbar(frame, mode="indeterminate", length=280)
optimal_progress.grid(row=8, column=0, columnspan=2, pady=(2, 2))

progress_status_label = tk.Label(frame, text="Sélectionnez une image et un mode pour activer la recherche.", fg="#444")
progress_status_label.grid(row=9, column=0, columnspan=2)

tk.Button(frame, text="Lancer le mode sélectionné", command=run_slic).grid(row=10, column=0, columnspan=2, pady=8)

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

mode_label = tk.Label(info_frame, text="Mode actif : -")
mode_label.grid(row=0, column=0, sticky="w", padx=4, pady=2)

psnr_label = tk.Label(info_frame, text="PSNR : -")
psnr_label.grid(row=1, column=0, sticky="w", padx=4, pady=2)

label_size = tk.Label(info_frame, text="Taille originale : - / compressée : -")
label_size.grid(row=2, column=0, sticky="w", padx=4, pady=2)

optimal_label = tk.Label(info_frame, text="Paramètres optimaux : -", justify="left")
optimal_label.grid(row=3, column=0, sticky="w", padx=4, pady=2)

refresh_optimal_controls()

root.mainloop()

#python code/src/UI.py dans l'env python