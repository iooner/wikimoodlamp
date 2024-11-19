import sseclient
import requests
import json
import time
import paho.mqtt.client as mqtt

# Configuration du flux Wikimedia
URL = "https://stream.wikimedia.org/v2/stream/recentchange"
IGNORE_LIST = []

# Configuration MQTT
MQTT_BROKER = ""  # Remplace par ton adresse
MQTT_PORT = 1883  # Port du broker
MQTT_USERNAME = ""  # Ton login
MQTT_PASSWORD = ""  # Ton mot de passe
MQTT_TOPIC = "wikipedia/raw"  # Unique topic pour publier les données

# Initialisation du client MQTT
mqtt_client = mqtt.Client()
mqtt_client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
try:
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    mqtt_client.loop_start()
except Exception as e:
    exit(1)

def reformat_page_title(title):
    """Transforme les titres pour un format plus lisible."""
    return title.replace(':', ' -> ').replace('/', ' -> ')

def get_ignore_reason(title):
    """Ignore les titres spécifiques dans IGNORE_LIST."""
    for ignore in IGNORE_LIST:
        if title.startswith(ignore):
            return ignore
    return None

def is_bot(user):
    """Détecte si l'utilisateur est un bot."""
    return user.lower().endswith('bot') or 'bot' in user.lower()

def listen_to_wikipedia_fr():
    while True:
        try:
            response = requests.get(URL, stream=True, timeout=10)
            client = sseclient.SSEClient(response)

            for event in client.events():
                try:
                    change = json.loads(event.data)

                    # Gestion des nouveaux utilisateurs pour fr.wikipedia.org
                    if change.get('server_name') == 'fr.wikipedia.org' and change.get('type') == 'new':
                        user = change.get('user', "inconnu")
                        mqtt_client.publish(MQTT_TOPIC, "user")
                        continue

                    # Gestion des modifications (type == 'edit')
                    if change.get('server_name') == 'fr.wikipedia.org' and change.get('type') == 'edit':
                        ignore_reason = get_ignore_reason(change['title'])
                        if ignore_reason:
                            continue
                        
                        user = change.get('user', "inconnu")
                        pseudo = user
                        page_title = reformat_page_title(change['title'])

                        # Condition spécifique pour "Vandalisme en cours"
                        if page_title == "Wikipédia -> Vandalisme en cours" or pseudo.lower() == "salebot":
                            mqtt_client.publish(MQTT_TOPIC, "vandalisme")
                        else:
                            # Calcul du changement en octets
                            length_info = change.get('length', {})
                            old_length = length_info.get('old', 0)
                            new_length = length_info.get('new', 0)
                            change_size = new_length - old_length

                            # Vérifier si c'est une édition mineure
                            minor_edit = change.get('minor', False)

                            if is_bot(pseudo):
                                mqtt_client.publish(MQTT_TOPIC, "bot")
                            else:
                                if minor_edit:
                                    if change_size > 0:
                                        mqtt_client.publish(MQTT_TOPIC, "plusminor")
                                    if change_size < 0:
                                        mqtt_client.publish(MQTT_TOPIC, "moinsminor")
                                else:
                                    if change_size > 0:
                                        mqtt_client.publish(MQTT_TOPIC, "plus")
                                    if change_size < 0:
                                        mqtt_client.publish(MQTT_TOPIC, "moins")

                except ValueError:
                    continue

        except requests.exceptions.RequestException as e:
            time.sleep(5)

if __name__ == "__main__":
    listen_to_wikipedia_fr()