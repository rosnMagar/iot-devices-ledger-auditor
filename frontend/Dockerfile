# syntax=docker/dockerfile:1
FROM --platform=$BUILDPLATFORM node:20-slim AS builder
WORKDIR /app
# No default. Vite bakes this into the bundle at build time, so a wrong value
# can't be corrected by restarting the container — it needs a rebuild. Defaulting
# to localhost meant a production image shipped pointing at the *viewer's* own
# machine, which fails in the browser with nothing wrong server-side.
ARG VITE_API_BASE_URL
ENV VITE_API_BASE_URL=$VITE_API_BASE_URL
COPY package.json ./
RUN npm install
COPY . .
RUN test -n "$VITE_API_BASE_URL" || { \
      echo "VITE_API_BASE_URL is empty — pass it with --build-arg. The bundle" >&2; \
      echo "would otherwise be built with no API address and fail in the browser." >&2; \
      exit 1; \
    }
RUN npm run build

FROM nginx:alpine
COPY --from=builder /app/dist /usr/share/nginx/html
EXPOSE 80
