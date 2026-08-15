package main

import (
	"flag"
	"log"
	"os"
	"os/signal"
	"syscall"

	"cbdos/router/pkg/router"
)

func main() {
	tcpAddr := flag.String("tcp", ":8765", "Dirección y puerto TCP para escuchar nodos Mesh / ESP32")
	unixSock := flag.String("unix", "/tmp/cbdos_router.sock", "Ruta del socket Unix para servicios locales (Hosting/Proxy)")
	hostingBackend := flag.String("hosting", "127.0.0.1:8766", "Dirección TCP del backend de Hosting (tlvgl_server.py)")
	flag.Parse()

	log.Println("=====================================================")
	log.Println("           CBDos Mesh Router (Go)                    ")
	log.Println("=====================================================")

	engine := router.NewRouterEngine()

	// 1. Iniciar Listener TCP (Red WiFi / Nodos)
	if err := engine.Listeners.AddTCPListener(*tcpAddr); err != nil {
		log.Fatalf("Error iniciando listener TCP: %v", err)
	}
	log.Printf("[Init] Listener TCP iniciado en %s", *tcpAddr)

	// 2. Iniciar Listener Unix Socket (Contenedores en el mismo host)
	if err := engine.Listeners.AddUnixListener(*unixSock); err != nil {
		log.Printf("[Init] ⚠ No se pudo crear el socket Unix (%v). Continuando solo TCP...", err)
	} else {
		log.Printf("[Init] Listener Unix Socket iniciado en %s", *unixSock)
	}

	// 3. Conectar opcionalmente con el servidor de Hosting backend si está disponible
	if *hostingBackend != "" {
		if err := engine.ConnectBackendService(0x07, *hostingBackend); err != nil {
			log.Printf("[Init] ℹ Backend de Hosting (%s) aún no está escuchando. Se registrará dinámicamente al conectarse.", *hostingBackend)
		}
	}

	// 4. Arrancar bucle de conexiones en segundo plano
	engine.Listeners.AcceptLoop(engine.HandleConn)


	log.Println("[Init] Router de Red Mesh activo y listo para procesar tramas.")
	log.Println("Presiona Ctrl+C para detener el servidor.")

	// Esperar señal de apagado
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	<-sigChan

	log.Println("\n[Shutdown] Apagando Router de Red Mesh...")
	engine.Listeners.Close()
	log.Println("[Shutdown] Router detenido correctamente.")
}
