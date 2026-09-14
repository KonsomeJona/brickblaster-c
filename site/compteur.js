// Compteur de visiteurs : un par navigateur. La première visite incrémente (POST), les
// suivantes lisent seulement (GET). Sans stockage local, on ne compte pas : chaque
// rechargement gonflerait le chiffre.
(() => {
    const sortie = document.getElementById('visites');
    const note = document.getElementById('visites-note');
    let dejaCompte = false, stockage = true;
    try {
        dejaCompte = localStorage.getItem('bb-visite') === '1';
        localStorage.setItem('bb-sonde', '1'); localStorage.removeItem('bb-sonde');
    } catch { stockage = false; }
    const methode = !dejaCompte && stockage ? 'POST' : 'GET';
    fetch('/api/visits', {method: methode}).then(reponse => {
        if (!reponse.ok) throw Error('compteur indisponible');
        return reponse.json();
    }).then(donnees => {
        if (!Number.isSafeInteger(donnees.visits) || donnees.visits < 0) throw Error('compte invalide');
        if (methode === 'POST') try { localStorage.setItem('bb-visite', '1'); } catch {}
        sortie.textContent = String(donnees.visits).padStart(6, '0');
    }).catch(() => {
        sortie.textContent = '------';
        note.textContent = 'Compteur momentanément indisponible.';
    });
})();
