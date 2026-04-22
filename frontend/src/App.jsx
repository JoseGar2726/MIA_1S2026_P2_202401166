import { useState, useRef, useEffect } from 'react'
import './App.css'

function App() {
  const [vistaActual, setVistaActual] = useState('home');
  
  const [usuarioLogeado, setUsuarioLogeado] = useState(null); 
  
  const [comandos, setComandos] = useState('');
  const [salida, setSalida] = useState('Esperando comandos...');
  const fileInputRef = useRef(null);

  const [loginForm, setLoginForm] = useState({ id: '', usuario: '', pass: '' });

  const [discosMontados, setDiscosMontados] = useState({});
  const [discoSeleccionado, setDiscoSeleccionado] = useState(null);
  const [particionSeleccionada, setParticionSeleccionada] = useState(null);

  const [rutaActual, setRutaActual] = useState('/');
  const [contenidoCarpeta, setContenidoCarpeta] = useState([]);

  const [archivoAbierto, setArchivoAbierto] = useState(null);
  const [textoArchivo, setTextoArchivo] = useState('');

  const [rutaEditada, setRutaEditada] = useState('/');

  const enviarPeticion = async (textoComando) => {
    try {
      const response = await fetch('http://localhost:8080/ejecutar', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ consola: textoComando })
      });
      const data = await response.json();
      return data.respuesta;
    } catch (error) {
      console.error(error);
      return "Error: No se pudo conectar con el servidor C++ en el puerto 8080.";
    }
  };

  useEffect(() => {
    if (vistaActual === 'visualizador' && !discoSeleccionado) {
      cargarDiscos();
    }
  }, [vistaActual, discoSeleccionado]);

  useEffect(() => {
    if (particionSeleccionada) {
      cargarArchivosDeRuta(rutaActual);
      setRutaEditada(rutaActual);
    }
  }, [particionSeleccionada, rutaActual]);

  const cargarDiscos = async () => {
    const respuesta = await enviarPeticion("mounted");
    
    let lineas = respuesta.split('\n').map(l => l.trim()).filter(l => l !== '');
    
    if (lineas.length === 0 || respuesta.toLowerCase().includes("error") || respuesta.includes("No hay")) {
      setDiscosMontados({});
      return;
    }

    const discosAgrupados = {};
    
    let currentID = "";
    let currentDisco = "";
    let currentParticion = "";

    lineas.forEach(linea => {
      if (linea.startsWith("ID:")) {
        currentID = linea.replace("ID:", "").trim();
      } 
      else if (linea.startsWith("Disco:")) {
        const rutaCompleta = linea.replace("Disco:", "").trim();
        const partes = rutaCompleta.split("/");
        currentDisco = partes[partes.length - 1];
      } 
      else if (linea.startsWith("Partición:") || linea.startsWith("Particion:")) {
        currentParticion = linea.substring(linea.indexOf(":") + 1).trim();
      } 
      else if (linea === "---") {
        if (currentID && currentDisco) {
          if (!discosAgrupados[currentDisco]) {
            discosAgrupados[currentDisco] = [];
          }
          discosAgrupados[currentDisco].push(`ID: ${currentID} | ${currentParticion}`);
        }
        currentID = ""; currentDisco = ""; currentParticion = "";
      }
    });

    if (currentID && currentDisco && currentParticion) {
        if (!discosAgrupados[currentDisco]) {
            discosAgrupados[currentDisco] = [];
        }
        if (!discosAgrupados[currentDisco].includes(`ID: ${currentID} | ${currentParticion}`)) {
            discosAgrupados[currentDisco].push(`ID: ${currentID} | ${currentParticion}`);
        }
    }

    setDiscosMontados(discosAgrupados);
  };

  const cargarArchivosDeRuta = async (ruta) => {
    const idParticion = particionSeleccionada.split('|')[0].replace('ID:', '').trim();

    try {
      const response = await fetch('http://localhost:8080/explorar', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: idParticion, ruta: ruta })
      });

      const data = await response.json();

      if (data.error) {
        console.error("Error del backend:", data.error);
        setContenidoCarpeta([]);
      } else {
        setContenidoCarpeta(data.archivos || []);
      }
    } catch (error) {
      console.error("Error al conectar con /explorar:", error);
      setContenidoCarpeta([]);
    }
  };

  const entrarACarpeta = (nombreCarpeta) => {
    const nuevaRuta = rutaActual === '/' ? `/${nombreCarpeta}` : `${rutaActual}/${nombreCarpeta}`;
    setRutaActual(nuevaRuta);
  };

  const subirDeNivel = () => {
    if (rutaActual === '/') return;
    const partes = rutaActual.split('/').filter(p => p !== '');
    partes.pop();
    const nuevaRuta = partes.length === 0 ? '/' : '/' + partes.join('/');
    setRutaActual(nuevaRuta);
  };

  const cargarArchivo = (event) => {
    const archivo = event.target.files[0];
    if (!archivo) return;
    const lector = new FileReader();
    lector.onload = (e) => setComandos(e.target.result);
    lector.readAsText(archivo);
    event.target.value = ""; 
  };

  const ejecutarComandosTerminal = async () => {
    if (!comandos.trim()) return alert("Ingresa un comando.");
    
    let lineasValidas = comandos.split('\n')
      .map(linea => linea.trim())
      .filter(linea => linea !== '')
      .join('\n');

    if (!lineasValidas) {
      return setSalida("No hay comandos válidos para ejecutar (solo habían comentarios o líneas vacías).");
    }

    setSalida("Ejecutando...");
    
    const respuesta = await enviarPeticion(lineasValidas);
    setSalida(respuesta);
  };

  const procesarLogin = async (e) => {
    e.preventDefault();
    if(!loginForm.id || !loginForm.usuario || !loginForm.pass) {
      return alert("Llena todos los campos");
    }

    try {
      const response = await fetch('http://localhost:8080/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ 
          usuario: loginForm.usuario, 
          pass: loginForm.pass, 
          id: loginForm.id 
        })
      });
      
      const data = await response.json();
      const respuesta = data.respuesta;
      
      if (respuesta.toLowerCase().includes("error")) {
        alert(respuesta);
      } else {
        alert("¡Login Exitoso!");
        
        setUsuarioLogeado(loginForm.usuario); 
        
        setVistaActual('visualizador');
        setLoginForm({ id: '', usuario: '', pass: '' });
      }
    } catch (error) {
      alert("Error crítico de conexión con el backend.");
    }
  };

  const procesarLogout = async () => {
    try {
      const response = await fetch('http://localhost:8080/logout', { 
        method: 'POST', 
        headers: { 'Content-Type': 'application/json' }, 
        body: JSON.stringify({}) 
      });
      const data = await response.json();
      alert(data.respuesta);
    } catch (error) { 
      console.warn("Backend no respondió, forzando cierre de sesión local."); 
    } finally {
      setUsuarioLogeado(null); 
      setDiscoSeleccionado(null); 
      setParticionSeleccionada(null); 
      setRutaActual('/');
      setVistaActual('home');
    }
  };

  const abrirArchivo = async (nombreArchivo) => {
    const idParticion = particionSeleccionada.split('|')[0].replace('ID:', '').trim();
    const rutaCompleta = rutaActual === '/' ? `/${nombreArchivo}` : `${rutaActual}/${nombreArchivo}`;

    try {
      const response = await fetch('http://localhost:8080/leer_archivo', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ id: idParticion, ruta: rutaCompleta })
      });

      const data = await response.json();

      if (data.error) {
        alert("Error: " + data.error);
      } else {
        setArchivoAbierto(nombreArchivo);
        setTextoArchivo(data.contenido);
      }
    } catch (error) {
      alert("Error crítico al leer el archivo.");
    }
  };

  if (vistaActual === 'login') {
    return (
      <>
        <nav className="navbar">
          <h1>EXT2/EXT3</h1>
          <button className="btn-nav btn-danger" onClick={() => setVistaActual('home')}>Regresar</button>
        </nav>
        <div className="login-container">
          <form className="login-box" onSubmit={procesarLogin}>
            <h2>Iniciar Sesión</h2>
            <input 
              type="text" placeholder="ID Partición (Ej. 661A)" 
              value={loginForm.id} onChange={e => setLoginForm({...loginForm, id: e.target.value})}
            />
            <input 
              type="text" placeholder="Usuario (Ej. root)" 
              value={loginForm.usuario} onChange={e => setLoginForm({...loginForm, usuario: e.target.value})}
            />
            <input 
              type="password" placeholder="Contraseña" 
              value={loginForm.pass} onChange={e => setLoginForm({...loginForm, pass: e.target.value})}
            />
            <button type="submit" className="btn-ejecutar">Ingresar</button>
          </form>
        </div>
      </>
    );
  }

  if (vistaActual === 'visualizador') {
    return (
      <>
        <nav className="navbar">
          <h1>Visualizador - 👤 {usuarioLogeado}</h1>
          <div className="nav-controls">
            <button className="btn-nav" style={{ backgroundColor: '#ff9800' }} onClick={() => { setVistaActual('home'); setDiscoSeleccionado(null); setParticionSeleccionada(null); setRutaActual('/'); }}>Terminal</button>
            <button className="btn-nav btn-danger" onClick={procesarLogout}>Cerrar Sesión</button>
          </div>
        </nav>
        
        <div className="visualizador-container">
          {!discoSeleccionado && !particionSeleccionada && (
            <>
              <h2>Discos Disponibles</h2>
              <button className="btn-nav" style={{ marginBottom: '20px' }} onClick={cargarDiscos}>↻ Refrescar Discos</button>
              <div className="card-grid">
                {Object.keys(discosMontados).length > 0 ? (
                  Object.keys(discosMontados).map((nombreDisco, index) => (
                    <div key={index} className="disk-card" onClick={() => setDiscoSeleccionado(nombreDisco)}>
                      <div className="disk-icon">🖴</div>
                      <p style={{ fontWeight: 'bold' }}>{nombreDisco}</p>
                      <p style={{ fontSize: '12px', color: '#888' }}>{discosMontados[nombreDisco].length} Partición(es)</p>
                    </div>
                  ))
                ) : <p style={{ color: '#aaa' }}>No hay discos montados.</p>}
              </div>
            </>
          )}

          {discoSeleccionado && !particionSeleccionada && (
            <div>
              <button className="btn-nav" style={{ marginBottom: '20px', backgroundColor: '#555' }} onClick={() => setDiscoSeleccionado(null)}>← Volver a Discos</button>
              <h2>Particiones en: <span style={{ color: '#00ff00' }}>{discoSeleccionado}</span></h2>
              <div className="card-grid">
                {discosMontados[discoSeleccionado].map((infoParticion, index) => (
                  <div key={index} className="disk-card" onClick={() => { setParticionSeleccionada(infoParticion); setRutaActual('/'); }} style={{ borderColor: '#007acc' }}>
                    <div className="disk-icon">🗂️</div>
                    <p style={{ fontSize: '11px', wordWrap: 'break-word' }}>{infoParticion}</p>
                  </div>
                ))}
              </div>
            </div>
          )}

          {discoSeleccionado && particionSeleccionada && (
            <div>
              <button className="btn-nav" style={{ marginBottom: '20px', backgroundColor: '#555' }} onClick={() => { 
                setParticionSeleccionada(null); 
                setRutaActual('/'); 
                setArchivoAbierto(null);
              }}>
                ← Volver a Particiones
              </button>
              
              <div style={{ backgroundColor: '#2d2d2d', padding: '15px', borderRadius: '8px', marginBottom: '20px', display: 'flex', alignItems: 'center', gap: '15px' }}>
                <button className="btn-nav" onClick={() => {
                  if (archivoAbierto) {
                    setArchivoAbierto(null);
                  } else {
                    subirDeNivel();
                  }
                }} disabled={rutaActual === '/' && !archivoAbierto} style={{ backgroundColor: (rutaActual === '/' && !archivoAbierto) ? '#555' : '#007acc', padding: '5px 10px' }}>
                  ⬅️ Regresar
                </button>

                <div style={{ flex: 1, display: 'flex', alignItems: 'center' }}>
                  <input 
                    type="text" 
                    value={rutaEditada}
                    onChange={(e) => setRutaEditada(e.target.value)}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter') {
                        let nuevaRuta = rutaEditada.trim();
                        if (!nuevaRuta.startsWith('/')) nuevaRuta = '/' + nuevaRuta;
                        if (nuevaRuta.endsWith('/') && nuevaRuta !== '/') nuevaRuta = nuevaRuta.slice(0, -1);
                        
                        setRutaActual(nuevaRuta);
                        setArchivoAbierto(null);
                      }
                    }}
                    style={{ 
                      flex: 1, 
                      backgroundColor: '#1e1e1e', 
                      padding: '10px', 
                      borderRadius: '4px', 
                      border: '1px solid #555', 
                      color: '#00ff00', 
                      fontFamily: 'monospace',
                      outline: 'none',
                      width: '100%'
                    }}
                  />
                  <button 
                    className="btn-nav" 
                    style={{ backgroundColor: '#4CAF50', marginLeft: '10px', padding: '10px 15px' }}
                    onClick={() => {
                        let nuevaRuta = rutaEditada.trim();
                        if (!nuevaRuta.startsWith('/')) nuevaRuta = '/' + nuevaRuta;
                        if (nuevaRuta.endsWith('/') && nuevaRuta !== '/') nuevaRuta = nuevaRuta.slice(0, -1);
                        
                        setRutaActual(nuevaRuta);
                        setArchivoAbierto(null);
                    }}
                  >
                    Ir ➔
                  </button>
                </div>
              </div>

              {archivoAbierto ? (
                <div style={{ padding: '0', border: '1px solid #555', backgroundColor: '#1e1e1e', borderRadius: '8px', overflow: 'hidden', display: 'flex', flexDirection: 'column' }}>
                  <div style={{ backgroundColor: '#333', padding: '10px 15px', borderBottom: '1px solid #555', fontWeight: 'bold' }}>
                    📄 {archivoAbierto}
                  </div>
                  <pre style={{ margin: 0, padding: '20px', color: '#e0e0e0', minHeight: '400px', whiteSpace: 'pre-wrap', fontFamily: 'monospace', fontSize: '14px' }}>
                    {textoArchivo || "El archivo está vacío."}
                  </pre>
                </div>
              ) : (
                <div style={{ padding: '20px', border: '1px solid #555', minHeight: '400px', backgroundColor: '#1e1e1e', borderRadius: '8px' }}>
                  {contenidoCarpeta.length === 0 ? (
                    <p style={{ color: '#777', textAlign: 'center', marginTop: '100px' }}>Esta carpeta está vacía.</p>
                  ) : (
                    <div className="card-grid" style={{ gap: '15px' }}>
                      {contenidoCarpeta.map((item, index) => (
                        <div 
                          key={index} 
                          className="disk-card" 
                          style={{ width: '100px', padding: '15px', borderColor: item.tipo === 'carpeta' ? '#ff9800' : '#4CAF50' }}
                          onClick={() => item.tipo === 'carpeta' ? entrarACarpeta(item.nombre) : abrirArchivo(item.nombre)}
                        >
                          <div className="disk-icon" style={{ fontSize: '40px' }}>
                            {item.tipo === 'carpeta' ? '📁' : '📄'}
                          </div>
                          <p style={{ fontSize: '12px', marginTop: '10px', overflow: 'hidden', textOverflow: 'ellipsis' }}>
                            {item.nombre}
                          </p>
                        </div>
                      ))}
                    </div>
                  )}
                </div>
              )}
            </div>
          )}

        </div>
      </>
    );
  }

  return (
    <>
      <nav className="navbar">
        <h1>EXT2/EXT3 {usuarioLogeado ? `| 👤 ${usuarioLogeado}` : ''}</h1>
        
        <div className="nav-controls">
          <input type="file" ref={fileInputRef} accept=".smia" style={{ display: 'none' }} onChange={cargarArchivo} />
          <button className="btn-nav" onClick={() => fileInputRef.current.click()}>Cargar Script</button>
          <button className="btn-nav btn-danger" onClick={() => { setComandos(''); setSalida('Esperando comandos...'); }}>Limpiar</button>
          
          {usuarioLogeado ? (
            <>
              <button className="btn-nav" style={{ backgroundColor: '#ff9800' }} onClick={() => setVistaActual('visualizador')}>Ver Archivos</button>
              <button className="btn-nav btn-danger" onClick={procesarLogout}>Cerrar Sesión</button>
            </>
          ) : (
            <button className="btn-nav" style={{ backgroundColor: '#007acc' }} onClick={() => setVistaActual('login')}>Iniciar Sesión</button>
          )}
        </div>
      </nav>

      <div className="container">
        <div className="editor">
          <h2>Ingreso de Comandos</h2>
          <textarea 
            value={comandos} onChange={(e) => setComandos(e.target.value)}
            spellCheck="false" placeholder="Escribe tus comandos aqui..."
          />
          <button className="btn-ejecutar" onClick={ejecutarComandosTerminal}>EJECUTAR COMANDOS</button>
        </div>
        <div className="consola">
          <h2>Salida del Sistema</h2>
          <pre>{salida}</pre>
        </div>
      </div>
    </>
  )
}

export default App